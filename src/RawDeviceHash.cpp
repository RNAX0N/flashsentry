#include "RawDeviceHash.h"
#include "RawDeviceHashAdvanced.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>

#include <cerrno>

#ifndef ENOTSUP
#define ENOTSUP EINVAL
#endif

#ifdef Q_OS_WIN

#include "WinStorage.h"
#include "RawDeviceHashAdvanced.h"

#include <openssl/evp.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QCoreApplication>

#include <shellapi.h>
#include <qt_windows.h>

#include <cerrno>
#include <cstring>

namespace FlashSentry::RawDeviceHash {

namespace {

const EVP_MD* mdFor(Algorithm algo)
{
    switch (algo) {
        case Algorithm::SHA512:
            return EVP_sha512();
        case Algorithm::BLAKE2b:
            return EVP_blake2b512();
        case Algorithm::SHA256:
        default:
            return EVP_sha256();
    }
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

HANDLE handleFromFd(int fd)
{
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd));
}

QString resolveDevicePath(const QString& deviceNode, int* errorCode)
{
    const QString trimmed = deviceNode.trimmed();
    if (WinStorage::isPhysicalDrivePath(trimmed)) {
        if (errorCode) {
            *errorCode = 0;
        }
        return QDir::toNativeSeparators(trimmed);
    }

    const QString physical = WinStorage::physicalDrivePathForDeviceNode(trimmed);
    if (!physical.isEmpty()) {
        if (errorCode) {
            *errorCode = 0;
        }
        return physical;
    }

    if (errorCode) {
        *errorCode = EINVAL;
    }
    return {};
}

QString defaultHelperPath()
{
#ifdef FLASHSENTRY_READ_HELPER_PATH
    return QString(FLASHSENTRY_READ_HELPER_PATH);
#else
    return QCoreApplication::applicationDirPath()
           + QStringLiteral("/flashsentry-read-helper.exe");
#endif
}

QString resolveHelperPath(const QString& helperPath)
{
    const QByteArray overridePath = qgetenv("FLASHSENTRY_READ_HELPER");
    if (!overridePath.isEmpty()) {
        return QString::fromLocal8Bit(overridePath);
    }
    if (!helperPath.isEmpty() && QFileInfo::exists(helperPath)) {
        return helperPath;
    }

    const QString sibling =
        QCoreApplication::applicationDirPath() + QStringLiteral("/flashsentry-read-helper.exe");
    if (QFileInfo::exists(sibling)) {
        return sibling;
    }

    const QString compiled = defaultHelperPath();
    if (QFileInfo::exists(compiled)) {
        return compiled;
    }
    return compiled;
}

HashResult hashReadLoopWin(HANDLE handle, const Options& options, uint64_t deviceSize)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx || EVP_DigestInit_ex(mdctx, mdFor(options.algorithm), nullptr) != 1) {
        if (mdctx) {
            EVP_MD_CTX_free(mdctx);
        }
        result.errorMessage = QStringLiteral("Failed to initialize hash algorithm");
        return result;
    }

    const size_t bufferSize =
        static_cast<size_t>(normalizedBufferSizeKB(options.bufferSizeKB)) * 1024;
    QByteArray buffer;
    buffer.resize(static_cast<int>(bufferSize));

    uint64_t totalRead = 0;
    while (totalRead < deviceSize) {
        if (cancelled(options)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Cancelled");
            return result;
        }

        LARGE_INTEGER pos{};
        pos.QuadPart = static_cast<LONGLONG>(totalRead);
        if (!SetFilePointerEx(handle, pos, nullptr, FILE_BEGIN)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage =
                QStringLiteral("Seek failed: Win32 error %1").arg(GetLastError());
            return result;
        }

        const DWORD toRead =
            static_cast<DWORD>(qMin<uint64_t>(bufferSize, deviceSize - totalRead));
        DWORD bytesRead = 0;
        if (!ReadFile(handle, buffer.data(), toRead, &bytesRead, nullptr) || bytesRead == 0) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Read error: Win32 error %1")
                                      .arg(GetLastError());
            return result;
        }

        if (EVP_DigestUpdate(mdctx, buffer.constData(), bytesRead) != 1) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Failed to update hash");
            return result;
        }

        totalRead += bytesRead;
        reportProgress(options, totalRead);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = QStringLiteral("Failed to finalize hash");
        return result;
    }

    QByteArray hashHex;
    hashHex.reserve(static_cast<int>(hashLen * 2));
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QStringLiteral("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }

    result.hash = QString::fromLatin1(hashHex);
    result.bytesProcessed = totalRead;
    result.success = true;
    EVP_MD_CTX_free(mdctx);
    return result;
}

HashResult hashViaElevatedHelper(const Options& options, const QString& helperPath)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    const QString path = resolveHelperPath(helperPath);
    if (!QFileInfo::exists(path)) {
        result.errorMessage =
            QStringLiteral("Privileged helper not found at %1").arg(path);
        return result;
    }

    QTemporaryFile tempFile(QDir::tempPath() + QStringLiteral("/flashsentry-hash-XXXXXX.json"));
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        result.errorMessage = QStringLiteral("Failed to create temporary result file");
        return result;
    }
    const QString outputPath = tempFile.fileName();
    tempFile.close();

    const QString params = QStringLiteral("hash \"%1\" %2 %3 %4 --output \"%5\"")
                             .arg(options.deviceNode,
                                  algorithmName(options.algorithm),
                                  QString::number(normalizedBufferSizeKB(options.bufferSizeKB)),
                                  options.useMemoryMapping ? QStringLiteral("1")
                                                           : QStringLiteral("0"),
                                  outputPath);

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(params.utf16());
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        const DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            result.errorMessage = QStringLiteral("Administrator approval was cancelled");
        } else {
            result.errorMessage =
                QStringLiteral("Failed to launch elevated helper (Win32 error %1)").arg(err);
        }
        return result;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }

    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("Elevated helper did not produce a result file");
        return result;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(outFile.readAll().trimmed());
    if (!doc.isObject()) {
        result.errorMessage = QStringLiteral("Invalid helper output");
        return result;
    }

    const QJsonObject obj = doc.object();
    if (!obj.value(QStringLiteral("success")).toBool()) {
        result.errorMessage =
            obj.value(QStringLiteral("error")).toString(QStringLiteral("Unknown error"));
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
        case Algorithm::SHA256:
            return QStringLiteral("SHA256");
        case Algorithm::SHA512:
            return QStringLiteral("SHA512");
        case Algorithm::BLAKE2b:
            return QStringLiteral("BLAKE2b");
    }
    return QStringLiteral("SHA256");
}

Algorithm algorithmFromName(const QString& name)
{
    const QString upper = name.toUpper();
    if (upper == QStringLiteral("SHA512")) {
        return Algorithm::SHA512;
    }
    if (upper == QStringLiteral("BLAKE2B") || upper == QStringLiteral("BLAKE2b")) {
        return Algorithm::BLAKE2b;
    }
    return Algorithm::SHA256;
}

int normalizedBufferSizeKB(int requestedKB)
{
    if (requestedKB <= 0) {
        return kDefaultBufferSizeKB;
    }
    if (requestedKB < kMinBufferSizeKB) {
        return kMinBufferSizeKB;
    }
    if (requestedKB > kMaxBufferSizeKB) {
        return kMaxBufferSizeKB;
    }
    return requestedKB;
}

int openDevice(const QString& deviceNode)
{
    int validationError = 0;
    const QString path = resolveDevicePath(deviceNode, &validationError);
    if (path.isEmpty()) {
        errno = validationError == 0 ? EINVAL : validationError;
        return -1;
    }

    DWORD winError = 0;
    HANDLE handle = WinStorage::openDeviceHandle(path, GENERIC_READ, &winError);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = (winError == ERROR_ACCESS_DENIED) ? EACCES : EINVAL;
        return -1;
    }

    return static_cast<int>(reinterpret_cast<intptr_t>(handle));
}

uint64_t deviceSize(int fd, const QString& /*deviceNode*/)
{
    if (fd < 0) {
        return 0;
    }
    return WinStorage::deviceSizeBytes(handleFromFd(fd), nullptr);
}

void closeDevice(int fd)
{
    if (fd >= 0) {
        WinStorage::closeDeviceHandle(handleFromFd(fd));
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

    return hashReadLoopWin(handleFromFd(fd), options, size);
}

HashResult hashDevice(const Options& options, const QString& pkexecHelperPath)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    const int fd = openDevice(options.deviceNode);
    if (fd >= 0) {
        result = hashOpenFd(fd, options);
        closeDevice(fd);
        return result;
    }

    if (errno == EACCES || errno == EPERM) {
        return hashViaElevatedHelper(options, pkexecHelperPath);
    }

    result.errorMessage =
        QStringLiteral("Failed to open device (Win32). Try running verification again to approve UAC.");
    return result;
}

} // namespace FlashSentry::RawDeviceHash

#else

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

bool isUnderDev(const QString& path)
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    return clean == QStringLiteral("/dev") || clean.startsWith(QStringLiteral("/dev/"));
}

QString validatedDevicePath(const QString& deviceNode, int* errorCode)
{
    const QString requested = QDir::cleanPath(QDir::fromNativeSeparators(deviceNode.trimmed()));
    if (!QDir::isAbsolutePath(requested) || !isUnderDev(requested)) {
        if (errorCode) *errorCode = EINVAL;
        return {};
    }

    const QFileInfo info(requested);
    if (!info.exists()) {
        if (errorCode) *errorCode = ENOENT;
        return {};
    }

    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || !isUnderDev(canonical)) {
        if (errorCode) *errorCode = EINVAL;
        return {};
    }

    if (errorCode) *errorCode = 0;
    return canonical;
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

    const size_t bufferSize = static_cast<size_t>(normalizedBufferSizeKB(options.bufferSizeKB)) * 1024;
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
        if (EVP_DigestUpdate(mdctx, buffer, static_cast<size_t>(bytesRead)) != 1) {
            free(buffer);
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = "Failed to update hash";
            return result;
        }
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
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        free(buffer);
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = "Failed to finalize hash";
        return result;
    }

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
        if (EVP_DigestUpdate(mdctx, mapped, mapSize) != 1) {
            munmap(mapped, mapSize);
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = "Failed to update hash";
            return result;
        }
        munmap(mapped, mapSize);

        offset += mapSize;
        reportProgress(options, offset);
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = "Failed to finalize hash";
        return result;
    }

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
        QString::number(normalizedBufferSizeKB(options.bufferSizeKB)),
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

int normalizedBufferSizeKB(int requestedKB)
{
    if (requestedKB <= 0) {
        return kDefaultBufferSizeKB;
    }
    if (requestedKB < kMinBufferSizeKB) {
        return kMinBufferSizeKB;
    }
    if (requestedKB > kMaxBufferSizeKB) {
        return kMaxBufferSizeKB;
    }
    return requestedKB;
}

int openDevice(const QString& deviceNode)
{
    int validationError = 0;
    const QString path = validatedDevicePath(deviceNode, &validationError);
    if (path.isEmpty()) {
        errno = validationError == 0 ? EINVAL : validationError;
        return -1;
    }

    const QByteArray encodedPath = QFile::encodeName(path);
    int fd = open(encodedPath.constData(), O_RDONLY | O_DIRECT | O_CLOEXEC);
    if (fd < 0) {
        fd = open(encodedPath.constData(), O_RDONLY | O_CLOEXEC);
    }
    if (fd < 0) {
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        const int savedErrno = errno;
        close(fd);
        errno = savedErrno;
        return -1;
    }
    if (!S_ISBLK(st.st_mode)) {
        close(fd);
        errno = ENOTBLK;
        return -1;
    }

    uint64_t size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) != 0) {
        const int savedErrno = errno == ENOTTY ? ENOTBLK : errno;
        close(fd);
        errno = savedErrno;
        return -1;
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

    const int tmpFd = openDevice(deviceNode);
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

#endif
