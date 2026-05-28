#include "RawDeviceHash.h"
#include "RawDeviceHashAdvanced.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>

#include <openssl/evp.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <cerrno>
#include <cstring>

namespace FlashSentry::RawDeviceHash {

namespace {

const EVP_MD* mdFor(Algorithm algo)
{
    switch (algo) {
        case Algorithm::SHA256: return EVP_sha256();
        case Algorithm::SHA512: return EVP_sha512();
        case Algorithm::BLAKE2b: return EVP_blake2b512();
    }
    return EVP_sha256();
}

bool cancelled(const Options& options)
{
    return options.cancelled && options.cancelled->load();
}

void reportProgress(const Options& options, uint64_t value)
{
    if (options.bytesProcessed) {
        options.bytesProcessed->store(value);
    }
}

HashResult hashReadLoop(int fd, const Options& options, uint64_t /*deviceSize*/)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        result.errorMessage = "Failed to create hash context";
        return result;
    }

    if (EVP_DigestInit_ex(mdctx, mdFor(options.algorithm), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = "Failed to initialize hash algorithm";
        return result;
    }

    const size_t bufferSize = static_cast<size_t>(options.bufferSizeKB) * 1024;
    void* buffer = nullptr;
    if (posix_memalign(&buffer, 4096, bufferSize) != 0) {
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = "Failed to allocate buffer";
        return result;
    }

    uint64_t totalRead = 0;
    ssize_t bytesRead = 0;

    while ((bytesRead = read(fd, buffer, bufferSize)) > 0) {
        if (cancelled(options)) {
            free(buffer);
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = "Cancelled";
            return result;
        }
        EVP_DigestUpdate(mdctx, buffer, static_cast<size_t>(bytesRead));
        totalRead += static_cast<uint64_t>(bytesRead);
        reportProgress(options, totalRead);
    }

    if (bytesRead < 0) {
        free(buffer);
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = QString("Read error: %1").arg(strerror(errno));
        return result;
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);

    QByteArray hashHex;
    hashHex.reserve(static_cast<int>(hashLen * 2));
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QString("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }

    result.hash = QString::fromLatin1(hashHex);
    result.bytesProcessed = totalRead;
    result.success = true;

    free(buffer);
    EVP_MD_CTX_free(mdctx);
    return result;
}

HashResult hashMmapLoop(int fd, const Options& options, uint64_t deviceSize)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    if (deviceSize == 0) {
        result.errorMessage = "Device size is 0";
        return result;
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        result.errorMessage = "Failed to create hash context";
        return result;
    }

    if (EVP_DigestInit_ex(mdctx, mdFor(options.algorithm), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = "Failed to initialize hash algorithm";
        return result;
    }

    const size_t chunkSize = 256 * 1024 * 1024;
    uint64_t offset = 0;

    while (offset < deviceSize) {
        if (cancelled(options)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = "Cancelled";
            return result;
        }

        const size_t mapSize = static_cast<size_t>(qMin(deviceSize - offset, static_cast<uint64_t>(chunkSize)));
        void* mapped = mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, fd, static_cast<off_t>(offset));
        if (mapped == MAP_FAILED) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QString("mmap failed: %1").arg(strerror(errno));
            return result;
        }

        madvise(mapped, mapSize, MADV_SEQUENTIAL);
        EVP_DigestUpdate(mdctx, mapped, mapSize);
        munmap(mapped, mapSize);

        offset += mapSize;
        reportProgress(options, offset);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);

    QByteArray hashHex;
    hashHex.reserve(static_cast<int>(hashLen * 2));
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QString("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }

    result.hash = QString::fromLatin1(hashHex);
    result.bytesProcessed = offset;
    result.success = true;

    EVP_MD_CTX_free(mdctx);
    return result;
}

QString defaultHelperPath()
{
#ifdef FLASHSENTRY_READ_HELPER_PATH
    return QString(FLASHSENTRY_READ_HELPER_PATH);
#else
    return QStringLiteral("/usr/lib/flashsentry/flashsentry-read-helper");
#endif
}


QString resolveHelperPath()
{
    const QByteArray overridePath = qgetenv("FLASHSENTRY_READ_HELPER");
    if (!overridePath.isEmpty()) {
        return QString::fromLocal8Bit(overridePath);
    }

    const QStringList candidates = {
        defaultHelperPath(),
        QStringLiteral("/usr/lib/flashsentry/flashsentry-read-helper"),
        QStringLiteral("/usr/libexec/flashsentry/flashsentry-read-helper"),
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return defaultHelperPath();
}

HashResult hashViaPkexec(const Options& options, const QString& helperPath)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    const QString path = helperPath.isEmpty() ? resolveHelperPath() : helperPath;

    QProcess proc;
    if (!QFileInfo::exists(path)) {
        result.errorMessage = QString(
            "Privileged helper not found at %1. Reinstall flashsentry or: sudo usermod -aG storage $USER")
            .arg(path);
        return result;
    }

    // pkexec matches org.flashsentry.read-raw-device via exec.path on the helper.
    proc.setProgram(QStringLiteral("pkexec"));
    proc.setArguments({
        path,
        QStringLiteral("hash"),
        options.deviceNode,
        algorithmName(options.algorithm),
        QString::number(options.bufferSizeKB),
        options.useMemoryMapping ? QStringLiteral("1") : QStringLiteral("0"),
    });
    proc.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    proc.start();
    if (!proc.waitForStarted(10000)) {
        result.errorMessage = QString("Failed to start pkexec: %1").arg(proc.errorString());
        return result;
    }

    while (proc.state() != QProcess::NotRunning) {
        if (cancelled(options)) {
            proc.kill();
            proc.waitForFinished(3000);
            result.errorMessage = "Cancelled";
            return result;
        }
        proc.waitForFinished(200);
    }

    const QByteArray stdoutData = proc.readAllStandardOutput().trimmed();
    const QByteArray stderrData = proc.readAllStandardError().trimmed();

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QString msg = QString::fromUtf8(stderrData);
        if (msg.isEmpty()) {
            msg = QString("Privileged hash failed (exit %1)").arg(proc.exitCode());
        }
        const QString lower = msg.toLower();
        if (lower.contains(QStringLiteral("not authorized"))
            || lower.contains(QStringLiteral("permission denied"))
            || lower.contains(QStringLiteral("cannot run"))) {
            msg += QStringLiteral(
                "\n\nEnsure flashsentry is reinstalled, a polkit agent is running in your "
                "desktop session (e.g. polkit-kde-agent or polkit-gnome), and try: "
                "sudo usermod -aG storage $USER (then log out and back in).");
        }
        result.errorMessage = msg;
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.errorMessage = QString("Invalid helper output: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject obj = doc.object();
    if (!obj.value(QStringLiteral("success")).toBool()) {
        result.errorMessage = obj.value(QStringLiteral("error")).toString(QStringLiteral("Unknown error"));
        return result;
    }

    result.hash = obj.value(QStringLiteral("hash")).toString();
    result.bytesProcessed = static_cast<uint64_t>(obj.value(QStringLiteral("bytes")).toDouble());
    result.success = true;
    reportProgress(options, result.bytesProcessed);
    return result;
}

} // namespace

QString algorithmName(Algorithm algo)
{
    switch (algo) {
        case Algorithm::SHA256: return QStringLiteral("SHA256");
        case Algorithm::SHA512: return QStringLiteral("SHA512");
        case Algorithm::BLAKE2b: return QStringLiteral("BLAKE2b");
    }
    return QStringLiteral("SHA256");
}

Algorithm algorithmFromName(const QString& name)
{
    const QString upper = name.toUpper();
    if (upper == QStringLiteral("SHA512")) return Algorithm::SHA512;
    if (upper == QStringLiteral("BLAKE2B") || upper == QStringLiteral("BLAKE2b")) {
        return Algorithm::BLAKE2b;
    }
    return Algorithm::SHA256;
}

int openDevice(const QString& deviceNode)
{
    int fd = open(deviceNode.toUtf8().constData(), O_RDONLY | O_DIRECT);
    if (fd < 0) {
        fd = open(deviceNode.toUtf8().constData(), O_RDONLY);
    }
    return fd;
}

uint64_t deviceSize(int fd, const QString& deviceNode)
{
    if (fd >= 0) {
        uint64_t size = 0;
        if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
            return size;
        }
        const off_t end = lseek(fd, 0, SEEK_END);
        if (end > 0) {
            return static_cast<uint64_t>(end);
        }
    }

    const int tmpFd = open(deviceNode.toUtf8().constData(), O_RDONLY);
    if (tmpFd < 0) {
        return 0;
    }
    const uint64_t size = deviceSize(tmpFd, deviceNode);
    close(tmpFd);
    return size;
}

void closeDevice(int fd)
{
    if (fd >= 0) {
        ::close(fd);
    }
}


HashResult hashOpenFd(int fd, const Options& options)
{
    HashResult result;
    result.deviceNode = options.deviceNode;

    const uint64_t size = deviceSize(fd, options.deviceNode);
    if (size == 0) {
        result.errorMessage = QStringLiteral("Device size is 0");
        return result;
    }

    if (options.scanMode == ScanMode::QuickSample || options.checkpointOut
        || options.resumeFromBytes > 0) {
        return hashAdvanced(fd, options, size);
    }

    if (options.useMemoryMapping && size > 0) {
        result = hashMmapLoop(fd, options, size);
        if (!result.success && result.errorMessage.contains(QStringLiteral("mmap"))) {
            result = hashReadLoop(fd, options, size);
        }
    } else {
        result = hashReadLoop(fd, options, size);
    }

    return result;
}

HashResult hashDevice(const Options& options, const QString& pkexecHelperPath)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    const int fd = openDevice(options.deviceNode);
    if (fd >= 0) {
        result = hashOpenFd(fd, options);
        close(fd);
        return result;
    }

    if (errno == EACCES || errno == EPERM) {
        return hashViaPkexec(options, pkexecHelperPath);
    }

    result.errorMessage = QString("Failed to open device: %1").arg(strerror(errno));
    return result;
}

} // namespace FlashSentry::RawDeviceHash
