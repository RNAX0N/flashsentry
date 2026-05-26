#include "IsoVerifier.h"
#include "AuditLog.h"
#include "IsoCatalog.h"
#include "IsoCatalogManifest.h"
#include "IsoChecksum.h"
#include "IsoVerifyCache.h"

#include <openssl/evp.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QtConcurrent>
#include <QThreadPool>

namespace FlashSentry {

namespace {

IsoVerifyOptions g_verifyOptions;

QString hashFileSha256(const QString& path, QString* errorOut);

QString hashDecompressedXz(const QString& path, QString* errorOut)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("xz"));
    proc.setArguments({QStringLiteral("-dc"), path});
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();
    if (!proc.waitForStarted(5000)) {
        if (errorOut) {
            *errorOut = QStringLiteral("xz decompressor not available (install xz)");
        }
        return {};
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    QByteArray buf(1024 * 1024, Qt::Uninitialized);
    while (proc.waitForReadyRead(30000)) {
        const qint64 n = proc.read(buf.data(), buf.size());
        if (n <= 0) {
            break;
        }
        EVP_DigestUpdate(ctx, buf.constData(), static_cast<size_t>(n));
    }
    proc.waitForFinished(3600000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        EVP_MD_CTX_free(ctx);
        if (errorOut) {
            *errorOut = proc.errorString().isEmpty() ? QStringLiteral("xz decompress failed")
                                                     : proc.errorString();
        }
        return {};
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);
    return QByteArray(reinterpret_cast<char*>(hash), static_cast<int>(len)).toHex();
}

QString computeFileSha256(const QString& path, const IsoVerifyOptions& options, QString* errorOut)
{
    const QFileInfo fi(path);
    if (options.useHashCache) {
        const QString cached =
            IsoVerifyCache::lookup(path, fi.size(), fi.lastModified().toMSecsSinceEpoch());
        if (!cached.isEmpty()) {
            return cached;
        }
    }

    QString hash;
    if (options.verifyDecompressed && path.endsWith(QStringLiteral(".img.xz"), Qt::CaseInsensitive)) {
        hash = hashDecompressedXz(path, errorOut);
    } else {
        hash = hashFileSha256(path, errorOut);
    }

    if (!hash.isEmpty() && options.useHashCache) {
        IsoVerifyCache::store(path, fi.size(), fi.lastModified().toMSecsSinceEpoch(), hash);
    }
    return hash;
}

QString normalizeHash(const QString& h)
{
    return h.trimmed().toLower();
}

QString hashFileSha256(const QString& path, QString* errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = file.errorString();
        return {};
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    QByteArray buf(1024 * 1024, Qt::Uninitialized);
    while (true) {
        const qint64 n = file.read(buf.data(), buf.size());
        if (n <= 0) break;
        EVP_DigestUpdate(ctx, buf.constData(), static_cast<size_t>(n));
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);

    return QByteArray(reinterpret_cast<char*>(hash), static_cast<int>(len)).toHex();
}

QString findChecksumSidecar(const QString& isoPath)
{
    const QFileInfo iso(isoPath);
    const QString base = iso.absolutePath() + QLatin1Char('/') + iso.completeBaseName();
    const QStringList candidates = {
        base + QStringLiteral(".sha256"),
        base + QStringLiteral(".sha256sum"),
        iso.absoluteFilePath() + QStringLiteral(".sha256"),
        iso.absoluteFilePath() + QStringLiteral(".sha256sum"),
        iso.absoluteFilePath() + QStringLiteral(".sha"),
        iso.absolutePath() + QStringLiteral("/SHA256SUMS"),
        iso.absolutePath() + QStringLiteral("/sha256sums.txt"),
        iso.absolutePath() + QStringLiteral("/sha256sum.txt"),
        iso.absolutePath() + QStringLiteral("/CHECKSUM"),
        iso.absoluteFilePath() + QStringLiteral(".CHECKSUM"),
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) return c;
    }
    return {};
}

QString findSignatureSidecar(const QString& isoPath)
{
    const QFileInfo iso(isoPath);
    const QString base = iso.absolutePath() + QLatin1Char('/') + iso.completeBaseName();
    const QStringList candidates = {
        base + QStringLiteral(".asc"),
        iso.absoluteFilePath() + QStringLiteral(".asc"),
        iso.absolutePath() + QStringLiteral("/SHA256SUMS.gpg"),
        iso.absolutePath() + QStringLiteral("/sha256sums.txt.sig"),
        iso.absolutePath() + QStringLiteral("/sha256sum.txt.gpg"),
        base + QStringLiteral(".sig"),
        iso.absoluteFilePath() + QStringLiteral(".sig"),
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) return c;
    }
    return {};
}

QString cacheDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                  + QStringLiteral("/iso-verify");
    QDir().mkpath(dir);
    return dir;
}

QStringList mirrorFallbackUrls(const QString& url)
{
    QStringList urls;
    urls << url;
    if (url.contains(QStringLiteral("geo.mirror.pkgbuild.com"))) {
        urls << url;
        urls.last().replace(QStringLiteral("geo.mirror.pkgbuild.com"),
                            QStringLiteral("mirror.pkgbuild.com"));
    }
    if (url.contains(QStringLiteral("download.rockylinux.org"))) {
        urls << url;
        urls.last().replace(QStringLiteral("download.rockylinux.org"),
                            QStringLiteral("dl.rockylinux.org"));
    }
    return urls;
}

QByteArray httpGet(const QString& url, QString* errorOut, int timeoutMs = 90000)
{
    const QStringList urls = mirrorFallbackUrls(url);
    QString lastErr;
    for (const QString& tryUrl : urls) {
        QNetworkAccessManager nam;
        QNetworkRequest req{QUrl(tryUrl)};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#ifdef FLASHSENTRY_VERSION
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("FlashSentry/" FLASHSENTRY_VERSION));
#else
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FlashSentry/1.1.5"));
#endif

        QNetworkReply* reply = nam.get(req);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();

        if (!reply->isFinished()) {
            lastErr = QStringLiteral("Download timed out");
            reply->abort();
            reply->deleteLater();
            continue;
        }
        if (reply->error() != QNetworkReply::NoError) {
            lastErr = reply->errorString();
            reply->deleteLater();
            continue;
        }

        const QByteArray data = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (status >= 400) {
            lastErr = QStringLiteral("HTTP %1 for %2").arg(status).arg(tryUrl);
            continue;
        }
        if (!data.isEmpty()) {
            return data;
        }
    }
    if (errorOut) {
        *errorOut = lastErr.isEmpty() ? QStringLiteral("HTTP download failed") : lastErr;
    }
    return {};
}

QString gpgHomedir()
{
    return cacheDir() + QStringLiteral("/gnupg");
}

bool ensureGpgHome(QString* errorOut)
{
    QDir dir(gpgHomedir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorOut) *errorOut = QStringLiteral("Cannot create GPG directory");
        return false;
    }
    return true;
}

QString runGpg(const QStringList& args, QString* outputOut, QString* errorOut, int timeoutMs = 120000)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("gpg"));
    QStringList fullArgs = {QStringLiteral("--homedir"), gpgHomedir()};
    fullArgs.append(args);
    proc.setArguments(fullArgs);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();
    if (!proc.waitForFinished(timeoutMs)) {
        if (errorOut) *errorOut = QStringLiteral("gpg timed out");
        return {};
    }
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    if (outputOut) *outputOut = out.trimmed();
    if (proc.exitCode() != 0) {
        if (errorOut) *errorOut = out.trimmed();
        return {};
    }
    return out.trimmed();
}

bool importPublisherKeys(const QStringList& keyIds, const QString& keyserver, QString* logOut)
{
    if (!ensureGpgHome(logOut)) return false;
    for (const QString& keyId : keyIds) {
        QString err;
        const QString out = runGpg(
            {QStringLiteral("--keyserver"), keyserver, QStringLiteral("--recv-keys"), keyId},
            logOut, &err, 180000);
        if (out.isEmpty() && !err.isEmpty()) {
            if (logOut) *logOut = err;
            return false;
        }
    }
    return true;
}

struct GpgVerifyDetails {
    bool valid = false;
    QString summary;
    QString keyId;
    QString fingerprint;
};

GpgVerifyDetails gpgVerifyDetached(const QString& sigPath, const QString& dataPath)
{
    GpgVerifyDetails d;
    QString output;
    QString err;
    runGpg({QStringLiteral("--verify"), sigPath, dataPath}, &output, &err);
    d.summary = output.isEmpty() ? err : output;

    static const QRegularExpression fpRe(
        QStringLiteral("(?:Primary key fingerprint:|key fingerprint is)\\s*([0-9A-Fa-f ]+)"));
    static const QRegularExpression keyRe(QStringLiteral("using \\w+ key ([0-9A-Fa-f]+)"));
    const QRegularExpressionMatch fm = fpRe.match(d.summary);
    if (fm.hasMatch()) {
        d.fingerprint = fm.captured(1).remove(QLatin1Char(' ')).toUpper();
    }
    const QRegularExpressionMatch km = keyRe.match(d.summary);
    if (km.hasMatch()) {
        d.keyId = km.captured(1);
    }

    d.valid = d.summary.contains(QStringLiteral("Good signature"), Qt::CaseInsensitive);
    return d;
}

bool fingerprintIsTrusted(const QString& fp, const QStringList& trusted)
{
    const QString norm = fp.trimmed().remove(QLatin1Char(' ')).toUpper();
    for (const QString& t : trusted) {
        if (norm == t.trimmed().remove(QLatin1Char(' ')).toUpper()) return true;
    }
    return false;
}

QString buildReport(const IsoVerifyResult& r)
{
    QStringList lines;
    lines << QStringLiteral("=== ISO Verification Report ===");
    lines << QStringLiteral("File: %1").arg(r.isoPath);
    if (!r.publisherName.isEmpty()) {
        lines << QStringLiteral("Publisher: %1 (%2)").arg(r.publisherName, r.releaseLabel);
    }
    if (!r.layoutNote.isEmpty()) lines << r.layoutNote;
    lines << QStringLiteral("SHA-256: %1").arg(r.computedSha256);
    if (!r.expectedSha256.isEmpty()) {
        lines << QStringLiteral("Expected: %1 [%2]")
                     .arg(r.expectedSha256, r.hashMatches ? QStringLiteral("MATCH") : QStringLiteral("MISMATCH"));
    }
    if (r.pgpChecked) {
        lines << QStringLiteral("OpenPGP: %1").arg(r.pgpValid ? QStringLiteral("valid") : QStringLiteral("FAILED"));
        if (!r.signingKeyFingerprint.isEmpty()) {
            lines << QStringLiteral("Signing key: %1 [fingerprint %2]")
                         .arg(r.signingKeyId, r.fingerprintTrusted ? QStringLiteral("trusted")
                                                                   : QStringLiteral("UNTRUSTED"));
        }
        if (!r.pgpSummary.isEmpty()) lines << r.pgpSummary;
    }
    if (!r.checksumUrl.isEmpty()) lines << QStringLiteral("Checksums: %1").arg(r.checksumUrl);
    lines << QStringLiteral("Result: %1").arg(r.passed() ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
    if (!r.errorMessage.isEmpty()) lines << QStringLiteral("Note: %1").arg(r.errorMessage);
    return lines.join(QLatin1Char('\n'));
}

} // namespace

IsoVerifier::MountScanResult IsoVerifier::scanMountPoint(const QString& mountPoint)
{
    MountScanResult scan;
    scan.mountPoint = mountPoint;
    scan.isoPaths = findIsoFiles(mountPoint);

    const QDir root(mountPoint);
    const bool hasArchiso = root.exists(QStringLiteral("arch"));
    const bool hasDiskInfo = root.exists(QStringLiteral(".disk/info"));
    const bool hasEfi = root.exists(QStringLiteral("EFI"));
    scan.looksLikeDdIsoStick = scan.isoPaths.isEmpty() && (hasArchiso || hasDiskInfo) && hasEfi;

    if (scan.looksLikeDdIsoStick) {
        scan.layoutNote = QStringLiteral(
            "Bootable live-USB layout detected (dd/hybrid write). No loose .iso file — "
            "use full-partition verification or copy the original .iso onto the drive for automated checks.");
    }
    return scan;
}

QStringList IsoVerifier::findIsoFiles(const QString& directory)
{
    QStringList result;
    QDirIterator it(directory, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (IsoCatalog::isVerifiableImageFileName(QFileInfo(path).fileName())) {
            result.append(path);
        }
    }
    result.sort();
    return result;
}

IsoVerifyResult IsoVerifier::verifyIso(const QString& isoPath, const QString& mountPoint,
                                       const QString& deviceNode)
{
    return verifyIsoAutomated(isoPath, mountPoint, deviceNode);
}

IsoVerifyResult IsoVerifier::verifyIsoAutomated(const QString& isoPath, const QString& mountPoint,
                                                const QString& deviceNode)
{
    IsoVerifyResult r;
    r.isoPath = isoPath;
    r.mountPoint = mountPoint;
    r.deviceNode = deviceNode;
    QElapsedTimer timer;
    timer.start();

    if (!QFileInfo::exists(isoPath)) {
        r.errorMessage = QStringLiteral("ISO not found");
        return r;
    }

    if (g_verifyOptions.cancelled && g_verifyOptions.cancelled->load()) {
        r.errorMessage = QStringLiteral("Cancelled");
        return r;
    }

    QString hashErr;
    r.computedSha256 = computeFileSha256(isoPath, g_verifyOptions, &hashErr);
    if (r.computedSha256.isEmpty()) {
        r.errorMessage = hashErr;
        return r;
    }
    r.hashChecked = true;

    const QFileInfo isoFi(isoPath);
    const QString isoName = isoFi.fileName();

    IsoCatalogManifest::refreshRemoteIfStale();

    if (g_verifyOptions.preferOfflineSidecars) {
        const QString checksumPath = findChecksumSidecar(isoPath);
        if (!checksumPath.isEmpty()) {
            QFile f(checksumPath);
            if (f.open(QIODevice::ReadOnly)) {
                QString parseErr;
                r.expectedSha256 =
                    IsoChecksum::parseSha256Content(QString::fromUtf8(f.readAll()), isoName, &parseErr);
                r.hashMatches = !r.expectedSha256.isEmpty()
                                && normalizeHash(r.computedSha256) == normalizeHash(r.expectedSha256);
                r.source = IsoVerifySource::LocalSidecar;
            }
        }
    }

    // 1) Try publisher catalog (remote, embedded, or hint)
    if (r.expectedSha256.isEmpty()) {
    if (auto match = IsoCatalog::matchIso(isoPath)) {
        r.publisherId = match->publisherId;
        r.publisherName = match->publisherName;
        r.releaseLabel = match->releaseLabel;
        r.trustedFingerprints = match->trustedFingerprints;
        r.checksumUrl = match->checksumUrl;
        r.signatureUrl = match->signatureUrl;

        if (!match->embeddedSha256.isEmpty()) {
            r.source = IsoVerifySource::EmbeddedCatalog;
            r.expectedSha256 = normalizeHash(match->embeddedSha256);
            r.hashMatches = normalizeHash(r.computedSha256) == r.expectedSha256;
        } else if (match->hintOnly) {
            r.source = IsoVerifySource::EmbeddedCatalog;
            if (!match->referenceUrl.isEmpty()) {
                r.checksumUrl = match->referenceUrl;
            }
        } else {
            r.source = IsoVerifySource::RemotePublisher;
        }

        QString fetchErr;
        const QByteArray sumsData = match->checksumUrl.isEmpty() || !match->embeddedSha256.isEmpty()
                                       ? QByteArray()
                                       : httpGet(match->checksumUrl, &fetchErr);
        if (!sumsData.isEmpty()) {
            r.remoteFetched = true;
            const QString sumsPath = cacheDir() + QLatin1Char('/') + match->publisherId
                                     + QStringLiteral("-SHA256SUMS.txt");
            QFile::remove(sumsPath);
            QFile sumsFile(sumsPath);
            if (sumsFile.open(QIODevice::WriteOnly)) {
                sumsFile.write(sumsData);
                sumsFile.close();
            }

            QString parseErr;
            r.expectedSha256 = IsoChecksum::parseSha256Content(QString::fromUtf8(sumsData), isoName, &parseErr);
            if (!r.expectedSha256.isEmpty()) {
                r.hashMatches = normalizeHash(r.computedSha256) == normalizeHash(r.expectedSha256);
            } else if (!parseErr.isEmpty()) {
                r.errorMessage = parseErr;
            }

            const QByteArray sigData = httpGet(match->signatureUrl, &fetchErr);
            if (!sigData.isEmpty()) {
                const QString sigSuffix = match->perFileArtifacts ? QStringLiteral("-iso.sig")
                                                                  : QStringLiteral("-SHA256SUMS.sig");
                const QString sigPath = cacheDir() + QLatin1Char('/') + match->publisherId + sigSuffix;
                QFile sigFile(sigPath);
                if (sigFile.open(QIODevice::WriteOnly)) {
                    sigFile.write(sigData);
                    sigFile.close();
                }

                r.keyserverUsed = QStringLiteral("hkps://keys.openpgp.org");
                QString importLog;
                if (importPublisherKeys(match->signingKeyIds, r.keyserverUsed, &importLog)) {
                    r.pgpChecked = true;
                    const QString signedDataPath = match->perFileArtifacts ? isoPath : sumsPath;
                    const GpgVerifyDetails vd = gpgVerifyDetached(sigPath, signedDataPath);
                    r.pgpValid = vd.valid;
                    r.pgpSummary = vd.summary;
                    r.signingKeyId = vd.keyId;
                    r.signingKeyFingerprint = vd.fingerprint;
                    r.signatureCoversChecksums = !match->perFileArtifacts && vd.valid;
                    r.fingerprintTrusted = match->trustedFingerprints.isEmpty()
                                               ? vd.valid
                                               : fingerprintIsTrusted(vd.fingerprint,
                                                                      match->trustedFingerprints);
                    if (vd.valid && !match->trustedFingerprints.isEmpty() && !r.fingerprintTrusted) {
                        r.errorMessage = QStringLiteral(
                            "Signature OK but signing key fingerprint is not in the trusted list");
                    }
                } else {
                    r.pgpSummary = importLog;
                }
            }
        } else if (r.errorMessage.isEmpty() && !match->hintOnly && !match->checksumUrl.isEmpty()) {
            r.errorMessage = QStringLiteral("Could not download publisher checksums: %1").arg(fetchErr);
        } else if (match->hintOnly && r.expectedSha256.isEmpty() && r.errorMessage.isEmpty()) {
            r.errorMessage = QStringLiteral(
                "Known image type — add a .sha256 sidecar next to the file or update the embedded "
                "catalog (see %1).")
                .arg(match->referenceUrl.isEmpty() ? QStringLiteral("docs/VERIFICATION.md")
                                                   : match->referenceUrl);
        }
    }
    }

    // 2) Local sidecars on drive (Rufus users often copy .sha256 + .asc alongside)
    if (r.expectedSha256.isEmpty()) {
        const QString checksumPath = findChecksumSidecar(isoPath);
        if (!checksumPath.isEmpty()) {
            QFile f(checksumPath);
            if (f.open(QIODevice::ReadOnly)) {
                QString parseErr;
                r.expectedSha256 = IsoChecksum::parseSha256Content(QString::fromUtf8(f.readAll()), isoName, &parseErr);
                r.hashMatches = !r.expectedSha256.isEmpty()
                                && normalizeHash(r.computedSha256) == normalizeHash(r.expectedSha256);
                r.source = IsoVerifySource::LocalSidecar;
            }
        }
    }

    if (r.expectedSha256.isEmpty()) {
        r.source = IsoVerifySource::ComputedOnly;
        r.hashMatches = true;
        r.reportSummary = QStringLiteral(
            "Computed SHA-256 only — unknown publisher, offline, or no checksum available. "
            "Add a .sha256 sidecar or update the catalog.");
    }

    const QString sigPath = findSignatureSidecar(isoPath);
    if (!sigPath.isEmpty() && !r.pgpChecked) {
        ensureGpgHome(nullptr);
        r.pgpChecked = true;
        const GpgVerifyDetails vd = gpgVerifyDetached(sigPath, isoPath);
        r.pgpValid = vd.valid;
        r.pgpSummary = vd.summary;
        r.signingKeyFingerprint = vd.fingerprint;
        r.signingKeyId = vd.keyId;
        if (!r.trustedFingerprints.isEmpty()) {
            r.fingerprintTrusted = fingerprintIsTrusted(vd.fingerprint, r.trustedFingerprints);
        } else {
            r.fingerprintTrusted = vd.valid;
        }
    }

    r.success = true;
    r.durationMs = static_cast<uint64_t>(timer.elapsed());
    r.reportSummary = buildReport(r);
    AuditLog::appendIsoVerify(r);
    return r;
}

namespace {

QList<IsoVerifyResult> verifyPathsParallel(const QStringList& paths, const QString& mountPoint,
                                           const QString& deviceNode)
{
    QList<IsoVerifyResult> results;
    if (paths.isEmpty()) {
        return results;
    }

    const int parallel = qMax(1, g_verifyOptions.maxParallel);
    QThreadPool pool;
    pool.setMaxThreadCount(parallel);

    QMutex mutex;
    results.resize(paths.size());

    auto verifyOne = [&](int index) {
        if (g_verifyOptions.cancelled && g_verifyOptions.cancelled->load()) {
            return;
        }
        if (g_verifyOptions.progress) {
            g_verifyOptions.progress(index + 1, paths.size(), QFileInfo(paths.at(index)).fileName());
        }
        IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(paths.at(index), mountPoint, deviceNode);
        QMutexLocker lock(&mutex);
        results[index] = r;
    };

    if (parallel <= 1 || paths.size() == 1) {
        for (int i = 0; i < paths.size(); ++i) {
            verifyOne(i);
        }
    } else {
        QList<QFuture<void>> futures;
        futures.reserve(paths.size());
        for (int i = 0; i < paths.size(); ++i) {
            futures.append(QtConcurrent::run(&pool, verifyOne, i));
        }
        for (QFuture<void>& f : futures) {
            f.waitForFinished();
        }
    }

    QList<IsoVerifyResult> ordered;
    for (const IsoVerifyResult& r : results) {
        if (!r.isoPath.isEmpty() || !r.layoutNote.isEmpty()) {
            ordered.append(r);
        }
    }
    return ordered;
}

} // namespace

IsoVerifyOptions& IsoVerifier::verifyOptions()
{
    return g_verifyOptions;
}

void IsoVerifier::setVerifyOptions(const IsoVerifyOptions& options)
{
    g_verifyOptions = options;
}

bool IsoVerifier::mountScanHasFailures(const QList<IsoVerifyResult>& results)
{
    for (const IsoVerifyResult& r : results) {
        if (!r.isoPath.isEmpty() && !r.passed()) {
            return true;
        }
    }
    return false;
}

QList<IsoVerifyResult> IsoVerifier::verifyDirectory(const QString& directory)
{
    return verifyPathsParallel(findIsoFiles(directory), directory, {});
}

QList<IsoVerifyResult> IsoVerifier::verifyMountPoint(const QString& mountPoint, const QString& deviceNode)
{
    QList<IsoVerifyResult> results;
    const MountScanResult scan = scanMountPoint(mountPoint);

    results = verifyPathsParallel(scan.isoPaths, mountPoint, deviceNode);
    for (IsoVerifyResult& r : results) {
        if (!scan.layoutNote.isEmpty() && r.layoutNote.isEmpty()) {
            r.layoutNote = scan.layoutNote;
        }
    }

    if (results.isEmpty() && scan.looksLikeDdIsoStick) {
        IsoVerifyResult note;
        note.success = true;
        note.mountPoint = mountPoint;
        note.deviceNode = deviceNode;
        note.layoutNote = scan.layoutNote;
        note.reportSummary = scan.layoutNote;
        note.source = IsoVerifySource::Unknown;
        results.append(note);
    }

    return results;
}

} // namespace FlashSentry
