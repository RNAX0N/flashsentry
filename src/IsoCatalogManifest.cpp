#include "IsoCatalogManifest.h"

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
    bool hintOnly = false;
    QRegularExpression regex;
};

QVector<ManifestEntry> g_entries;
int g_manifestVersion = 0;
QString g_remoteUrl;
bool g_loaded = false;

QString manifestCachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/iso-catalog-manifest.json");
}

bool parseManifestDocument(const QJsonDocument& doc, QString* errorOut)
{
    if (!doc.isObject()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Manifest root must be a JSON object");
        }
        return false;
    }

    const QJsonObject root = doc.object();
    g_manifestVersion = root.value(QStringLiteral("manifest_version")).toInt(0);
    g_remoteUrl = root.value(QStringLiteral("remote_url")).toString();

    QVector<ManifestEntry> parsed;
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    parsed.reserve(entries.size());

    for (const QJsonValue& val : entries) {
        const QJsonObject obj = val.toObject();
        ManifestEntry e;
        e.publisherId = obj.value(QStringLiteral("publisher_id")).toString();
        e.publisherName = obj.value(QStringLiteral("publisher_name")).toString();
        e.filePattern = obj.value(QStringLiteral("file_pattern")).toString();
        e.releaseLabel = obj.value(QStringLiteral("release_label")).toString();
        e.sha256 = obj.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        e.referenceUrl = obj.value(QStringLiteral("reference_url")).toString();
        e.hintOnly = obj.value(QStringLiteral("hint_only")).toBool(false);
        if (e.publisherId.isEmpty() || e.filePattern.isEmpty()) {
            continue;
        }
        e.regex = QRegularExpression(e.filePattern, QRegularExpression::CaseInsensitiveOption);
        if (!e.regex.isValid()) {
            continue;
        }
        parsed.append(e);
    }

    g_entries = parsed;
    return true;
}

QByteArray readResourceManifest()
{
    QFile f(QStringLiteral(":/iso-catalog/iso-catalog/embedded-manifest.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

void loadFromBytes(const QByteArray& data)
{
    QString err;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!parseManifestDocument(doc, &err)) {
        g_entries.clear();
    }
}

} // namespace

void IsoCatalogManifest::ensureLoaded()
{
    if (g_loaded) {
        return;
    }
    g_loaded = true;

    const QString cachePath = manifestCachePath();
    if (QFileInfo::exists(cachePath)) {
        QFile cache(cachePath);
        if (cache.open(QIODevice::ReadOnly)) {
            loadFromBytes(cache.readAll());
            if (!g_entries.isEmpty()) {
                return;
            }
        }
    }

    loadFromBytes(readResourceManifest());
}

bool IsoCatalogManifest::refreshRemoteIfStale(int maxAgeSeconds)
{
    ensureLoaded();
    if (g_remoteUrl.isEmpty()) {
        return false;
    }

    const QString cachePath = manifestCachePath();
    if (QFileInfo::exists(cachePath)) {
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

    QString err;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!parseManifestDocument(doc, &err)) {
        return false;
    }

    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    QFile cache(cachePath);
    if (cache.open(QIODevice::WriteOnly)) {
        cache.write(body);
        cache.close();
    }
    return true;
}

std::optional<IsoPublisherMatch> IsoCatalogManifest::lookup(const QString& fileName)
{
    ensureLoaded();

    for (const ManifestEntry& e : g_entries) {
        const QRegularExpressionMatch m = e.regex.match(fileName);
        if (!m.hasMatch()) {
            continue;
        }

        IsoPublisherMatch match;
        match.publisherId = e.publisherId;
        match.publisherName = e.publisherName;
        match.releaseLabel = e.releaseLabel;
        match.isoFileName = fileName;
        match.embeddedSha256 = e.sha256;
        match.hintOnly = e.hintOnly;
        match.referenceUrl = e.referenceUrl;
        return match;
    }
    return std::nullopt;
}

int IsoCatalogManifest::entryCount()
{
    ensureLoaded();
    return g_entries.size();
}

} // namespace FlashSentry
