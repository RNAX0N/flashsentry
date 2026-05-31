#include "IsoCatalogManifest.h"
#include "GpgUtil.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QEventLoop>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

namespace FlashSentry {

namespace {

struct ManifestEntry {
    QString publisherId;
    QString publisherName;
    QString filePattern;
    QString releaseLabel;
    QString sha256;
    QString referenceUrl;
    QString checksumUrlTemplate;
    QString signatureUrlTemplate;
    QStringList signingKeyIds;
    QStringList trustedFingerprints;
    bool hintOnly = false;
    bool userTofu = false;
    QRegularExpression regex;
};

QVector<ManifestEntry> g_entries;
int g_manifestVersion = 0;
QString g_remoteUrl;
bool g_loaded = false;
bool g_embeddedShaOk = true;
bool g_embeddedGpgOk = true;
QString g_embeddedShaDetail;
QString g_embeddedGpgDetail;

QString manifestCachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/iso-catalog-manifest.json");
}

QString userTofuPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/user-iso-hashes.json");
}

QStringList catalogDropInDirs()
{
    QStringList dirs;
    dirs << QStringLiteral("/usr/share/flashsentry/iso-catalog.d");
    dirs << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
              + QStringLiteral("/iso-catalog.d");
    return dirs;
}

bool parseEntryObject(const QJsonObject& obj, ManifestEntry* out, bool userTofu = false)
{
    ManifestEntry e;
    e.publisherId = obj.value(QStringLiteral("publisher_id")).toString();
    e.publisherName = obj.value(QStringLiteral("publisher_name")).toString();
    e.filePattern = obj.value(QStringLiteral("file_pattern")).toString();
    e.releaseLabel = obj.value(QStringLiteral("release_label")).toString();
    e.sha256 = obj.value(QStringLiteral("sha256")).toString().trimmed().toLower();
    e.referenceUrl = obj.value(QStringLiteral("reference_url")).toString();
    e.checksumUrlTemplate = obj.value(QStringLiteral("checksum_url_template")).toString();
    e.signatureUrlTemplate = obj.value(QStringLiteral("signature_url_template")).toString();
    const QJsonArray keyIds = obj.value(QStringLiteral("signing_key_ids")).toArray();
    for (const QJsonValue& kid : keyIds) {
        const QString id = kid.toString().trimmed();
        if (!id.isEmpty()) {
            e.signingKeyIds.append(id);
        }
    }
    const QJsonArray fps = obj.value(QStringLiteral("trusted_fingerprints")).toArray();
    for (const QJsonValue& fp : fps) {
        QString f = fp.toString().trimmed();
        if (!f.isEmpty()) {
            f.remove(QLatin1Char(' '));
            e.trustedFingerprints.append(f.toUpper());
        }
    }
    e.hintOnly = obj.value(QStringLiteral("hint_only")).toBool(false);
    e.userTofu = userTofu;
    if (e.publisherId.isEmpty() || e.filePattern.isEmpty()) {
        return false;
    }
    e.regex = QRegularExpression(e.filePattern, QRegularExpression::CaseInsensitiveOption);
    if (!e.regex.isValid()) {
        return false;
    }
    *out = e;
    return true;
}

void mergeManifestDocument(const QJsonDocument& doc, bool prependUserTofu = false)
{
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject root = doc.object();
    if (g_manifestVersion == 0) {
        g_manifestVersion = root.value(QStringLiteral("manifest_version")).toInt(0);
    }
    if (g_remoteUrl.isEmpty()) {
        g_remoteUrl = root.value(QStringLiteral("remote_url")).toString();
    }

    QVector<ManifestEntry> parsed;
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& val : entries) {
        ManifestEntry e;
        if (parseEntryObject(val.toObject(), &e, prependUserTofu)) {
            parsed.append(e);
        }
    }
    if (prependUserTofu) {
        g_entries = parsed + g_entries;
    } else {
        g_entries += parsed;
    }
}

bool verifyEmbeddedSha256(const QByteArray& manifestBytes)
{
    QFile hashFile(QStringLiteral(":/iso-catalog/iso-catalog/embedded-manifest.json.sha256"));
    if (!hashFile.open(QIODevice::ReadOnly)) {
        return true;
    }
    const QString expected = QString::fromUtf8(hashFile.readAll()).trimmed().left(64).toLower();
    if (expected.size() != 64) {
        return true;
    }
    const QByteArray digest =
        QCryptographicHash::hash(manifestBytes, QCryptographicHash::Sha256).toHex();
    const QString computed = QString::fromLatin1(digest);
    if (computed == expected) {
        g_embeddedShaDetail.clear();
        return true;
    }
    g_embeddedShaDetail =
        QStringLiteral("computed %1 expected %2").arg(computed, expected);
    return false;
}

bool verifyEmbeddedGpgWithPaths(const QString& gpgHome, const QString& pubPath,
                                const QString& sigPath, const QString& manifestPath)
{
    auto runGpgInHome = [&](const QStringList& args, QString* output) -> bool {
        QProcess p;
        configureGpgProcess(p);
        QStringList fullArgs = gpgBatchArgs();
        fullArgs << QStringLiteral("--homedir") << QDir::toNativeSeparators(gpgHome);
        fullArgs.append(args);
        p.setArguments(fullArgs);
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start();
        if (!p.waitForStarted(10000)) {
            g_embeddedGpgDetail = QStringLiteral("gpg failed to start (%1)").arg(gpgProgram());
            return false;
        }
        if (!p.waitForFinished(30000)) {
            p.kill();
            g_embeddedGpgDetail = QStringLiteral("gpg timed out");
            return false;
        }
        const QByteArray combined = p.readAll();
        if (output) {
            *output = QString::fromUtf8(combined);
        }
        if (p.exitCode() != 0) {
            g_embeddedGpgDetail = QString::fromUtf8(combined).trimmed();
            if (g_embeddedGpgDetail.isEmpty()) {
                g_embeddedGpgDetail = QStringLiteral("gpg exit code %1").arg(p.exitCode());
            }
            return false;
        }
        return true;
    };

    g_embeddedGpgDetail.clear();
    if (!runGpgInHome({QStringLiteral("--import"), QDir::toNativeSeparators(pubPath)}, nullptr)) {
        return false;
    }
    return runGpgInHome({QStringLiteral("--verify"), QDir::toNativeSeparators(sigPath),
                         QDir::toNativeSeparators(manifestPath)},
                        nullptr);
}

bool verifyEmbeddedGpgOnDisk()
{
    const QString root = gpgScratchRoot();
    const QString manifestPath =
        root + QStringLiteral("/resources/iso-catalog/embedded-manifest.json");
    const QString sigPath =
        root + QStringLiteral("/resources/iso-catalog/embedded-manifest.json.asc");
    const QString pubPath = root + QStringLiteral("/resources/iso-catalog/catalog-signing.pub");
    if (!QFile::exists(manifestPath) || !QFile::exists(sigPath) || !QFile::exists(pubPath)) {
        return false;
    }

    const QString gpgHome =
        root + QStringLiteral("/.flashsentry-embedded-gpg-home");
    if (!QDir().mkpath(gpgHome)) {
        g_embeddedGpgDetail = QStringLiteral("cannot create gpg homedir");
        return false;
    }
    return verifyEmbeddedGpgWithPaths(gpgHome, pubPath, sigPath, manifestPath);
}

bool verifyEmbeddedGpgSignature(const QByteArray& manifestBytes)
{
    if (verifyEmbeddedGpgOnDisk()) {
        return true;
    }

    QFile sigFile(QStringLiteral(":/iso-catalog/iso-catalog/embedded-manifest.json.asc"));
    QFile pubFile(QStringLiteral(":/iso-catalog/iso-catalog/catalog-signing.pub"));
    if (!sigFile.exists() || !pubFile.exists()) {
        return true;
    }
    if (!sigFile.open(QIODevice::ReadOnly) || !pubFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray sig = sigFile.readAll();
    const QByteArray pub = pubFile.readAll();
    if (sig.isEmpty() || pub.isEmpty()) {
        return false;
    }

    const QString scratchRoot = gpgScratchRoot() + QStringLiteral("/.flashsentry-gpg-scratch");
    QDir().mkpath(scratchRoot);
    QTemporaryDir temp(scratchRoot + QStringLiteral("/verify-XXXXXX"));
    if (!temp.isValid()) {
        return false;
    }
    const QString manifestPath = temp.filePath(QStringLiteral("embedded-manifest.json"));
    const QString sigPath = temp.filePath(QStringLiteral("embedded-manifest.json.asc"));
    const QString pubPath = temp.filePath(QStringLiteral("catalog-signing.pub"));

    QFile manifestOut(manifestPath);
    QFile sigOut(sigPath);
    QFile pubOut(pubPath);
    if (!manifestOut.open(QIODevice::WriteOnly) || !sigOut.open(QIODevice::WriteOnly)
        || !pubOut.open(QIODevice::WriteOnly)) {
        return false;
    }
    manifestOut.write(manifestBytes);
    sigOut.write(sig);
    pubOut.write(pub);
    manifestOut.close();
    sigOut.close();
    pubOut.close();

    const QString gpgHome = temp.filePath(QStringLiteral("gnupg"));
    if (!QDir().mkpath(gpgHome)) {
        return false;
    }
    return verifyEmbeddedGpgWithPaths(gpgHome, pubPath, sigPath, manifestPath);
}

void loadFromFile(const QString& path, bool userTofu = false)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    mergeManifestDocument(QJsonDocument::fromJson(f.readAll()), userTofu);
}

void loadCatalogDropIns()
{
    for (const QString& dirPath : catalogDropInDirs()) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            continue;
        }
        const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QString& name : files) {
            loadFromFile(dir.absoluteFilePath(name));
        }
    }
}

void reloadAll()
{
    g_entries.clear();
    g_manifestVersion = 0;
    g_remoteUrl.clear();
    g_embeddedShaOk = true;
    g_embeddedGpgOk = true;
    g_embeddedShaDetail.clear();
    g_embeddedGpgDetail.clear();

    QFile embedded(QStringLiteral(":/iso-catalog/iso-catalog/embedded-manifest.json"));
    if (embedded.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = embedded.readAll();
        g_embeddedShaOk = verifyEmbeddedSha256(bytes);
        g_embeddedGpgOk = verifyEmbeddedGpgSignature(bytes);
        mergeManifestDocument(QJsonDocument::fromJson(bytes));
    }

    const QString cachePath = manifestCachePath();
    if (QFileInfo::exists(cachePath)) {
        loadFromFile(cachePath);
    }

    loadCatalogDropIns();
    loadFromFile(userTofuPath(), true);
}

IsoPublisherMatch entryToMatch(const ManifestEntry& e, const QString& fileName)
{
    IsoPublisherMatch match;
    match.publisherId = e.publisherId;
    match.publisherName = e.publisherName;
    match.releaseLabel = e.releaseLabel;
    match.isoFileName = fileName;
    match.embeddedSha256 = e.sha256;
    match.hintOnly = e.hintOnly;
    match.referenceUrl = e.referenceUrl;
    if (!e.checksumUrlTemplate.isEmpty()) {
        match.checksumUrl = e.checksumUrlTemplate;
        match.checksumUrl.replace(QStringLiteral("{filename}"), fileName);
    }
    if (!e.signatureUrlTemplate.isEmpty()) {
        match.signatureUrl = e.signatureUrlTemplate;
        match.signatureUrl.replace(QStringLiteral("{filename}"), fileName);
    }
    match.signingKeyIds = e.signingKeyIds;
    match.trustedFingerprints = e.trustedFingerprints;
    return match;
}

} // namespace

void IsoCatalogManifest::reload()
{
    g_loaded = true;
    reloadAll();
}

void IsoCatalogManifest::ensureLoaded()
{
    if (!g_loaded) {
        reload();
    }
}

bool IsoCatalogManifest::lastEmbeddedIntegrityOk()
{
    ensureLoaded();
    return g_embeddedShaOk && g_embeddedGpgOk;
}

bool IsoCatalogManifest::lastEmbeddedSha256Ok()
{
    ensureLoaded();
    return g_embeddedShaOk;
}

bool IsoCatalogManifest::lastEmbeddedGpgOk()
{
    ensureLoaded();
    return g_embeddedGpgOk;
}

QString IsoCatalogManifest::integrityStatusText()
{
    ensureLoaded();
    if (g_embeddedShaOk && g_embeddedGpgOk) {
        return QStringLiteral("Embedded ISO catalog integrity OK (%1 manifest entries).")
            .arg(entryCount());
    }
    QStringList issues;
    if (!g_embeddedShaOk) {
        if (g_embeddedShaDetail.isEmpty()) {
            issues << QStringLiteral("SHA-256 digest mismatch");
        } else {
            issues << g_embeddedShaDetail;
        }
    }
    if (!g_embeddedGpgOk) {
        if (g_embeddedGpgDetail.isEmpty()) {
            issues << QStringLiteral("OpenPGP signature check failed (missing gpg or invalid signature)");
        } else {
            issues << g_embeddedGpgDetail;
        }
    }
    return QStringLiteral("Embedded catalog integrity failed: %1")
        .arg(issues.join(QStringLiteral("; ")));
}

bool IsoCatalogManifest::refreshRemoteIfStale(int maxAgeSeconds, bool force)
{
    ensureLoaded();
    if (qEnvironmentVariableIsSet("FLASHSENTRY_SKIP_REMOTE_CATALOG")) {
        return true;
    }
    if (g_remoteUrl.isEmpty()) {
        return false;
    }

    const QString cachePath = manifestCachePath();
    if (!force && QFileInfo::exists(cachePath)) {
        const qint64 age = QFileInfo(cachePath).lastModified().secsTo(QDateTime::currentDateTime());
        if (age >= 0 && age < maxAgeSeconds) {
            return true;
        }
    }

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(g_remoteUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
#ifdef FLASHSENTRY_VERSION
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("FlashSentry/" FLASHSENTRY_VERSION));
#endif

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return false;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (body.isEmpty()) {
        return false;
    }

    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    QFile cache(cachePath);
    if (cache.open(QIODevice::WriteOnly)) {
        cache.write(body);
        cache.close();
    }

    reload();
    return true;
}

std::optional<IsoPublisherMatch> IsoCatalogManifest::lookup(const QString& fileName)
{
    ensureLoaded();

    for (const ManifestEntry& e : g_entries) {
        if (!e.regex.match(fileName).hasMatch()) {
            continue;
        }
        return entryToMatch(e, fileName);
    }
    return std::nullopt;
}

int IsoCatalogManifest::entryCount()
{
    ensureLoaded();
    return g_entries.size();
}

bool IsoCatalogManifest::trustUserHash(const QString& fileName, const QString& sha256Hex)
{
    const QString hash = sha256Hex.trimmed().toLower();
    if (hash.size() != 64) {
        return false;
    }

    QJsonArray entries;
    QFile existing(userTofuPath());
    if (existing.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(existing.readAll()).object();
        entries = root.value(QStringLiteral("entries")).toArray();
        existing.close();
    }

    QJsonArray filtered;
    const QString pattern = QStringLiteral("^%1$")
                                .arg(QRegularExpression::escape(fileName));
    for (const QJsonValue& v : entries) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("file_pattern")).toString() != pattern) {
            filtered.append(o);
        }
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("publisher_id"), QStringLiteral("user-trusted"));
    entry.insert(QStringLiteral("publisher_name"), QStringLiteral("User trusted"));
    entry.insert(QStringLiteral("file_pattern"), pattern);
    entry.insert(QStringLiteral("release_label"), fileName);
    entry.insert(QStringLiteral("sha256"), hash);
    filtered.append(entry);

    QJsonObject root;
    root.insert(QStringLiteral("manifest_version"), 1);
    root.insert(QStringLiteral("entries"), filtered);

    QFile out(userTofuPath());
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();

    reload();
    return true;
}

} // namespace FlashSentry
