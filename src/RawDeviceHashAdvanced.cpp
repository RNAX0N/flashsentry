#include "RawDeviceHashAdvanced.h"

#include <openssl/evp.h>

#include <QByteArray>
#include <QVector>

#ifdef Q_OS_WIN
#include <qt_windows.h>

namespace FlashSpartan::RawDeviceHash {

QString scanModeTag(ScanMode mode)
{
    return mode == ScanMode::QuickSample ? QStringLiteral("quick") : QStringLiteral("full");
}

QString combineBlockHashes(const QStringList& blockHex, Algorithm algo)
{
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return {};
    }
    const EVP_MD* md = nullptr;
    switch (algo) {
        case Algorithm::SHA512:
            md = EVP_sha512();
            break;
        case Algorithm::BLAKE2b:
            md = EVP_blake2b512();
            break;
        case Algorithm::SHA256:
        default:
            md = EVP_sha256();
            break;
    }
    if (!md || EVP_DigestInit_ex(mdctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return {};
    }
    for (const QString& hex : blockHex) {
        const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
        EVP_DigestUpdate(mdctx, raw.constData(), static_cast<size_t>(raw.size()));
    }
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    QString out;
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) == 1) {
        QByteArray hashHex;
        hashHex.reserve(static_cast<int>(hashLen * 2));
        for (unsigned int i = 0; i < hashLen; ++i) {
            hashHex.append(QStringLiteral("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
        }
        out = QString::fromLatin1(hashHex);
    }
    EVP_MD_CTX_free(mdctx);
    return out;
}

namespace {

HANDLE handleFromFd(int fd)
{
    return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd));
}

bool winSeek(HANDLE handle, uint64_t offset)
{
    LARGE_INTEGER pos{};
    pos.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(handle, pos, nullptr, FILE_BEGIN) != 0;
}

bool winRead(HANDLE handle, char* buffer, size_t toRead, size_t* bytesReadOut)
{
    DWORD read = 0;
    if (!ReadFile(handle, buffer, static_cast<DWORD>(toRead), &read, nullptr)) {
        return false;
    }
    if (bytesReadOut) {
        *bytesReadOut = static_cast<size_t>(read);
    }
    return read > 0;
}

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

QString digestToHex(const unsigned char* hash, unsigned int hashLen)
{
    QByteArray hashHex;
    hashHex.reserve(static_cast<int>(hashLen * 2));
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QStringLiteral("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }
    return QString::fromLatin1(hashHex);
}

bool finalizeCtx(EVP_MD_CTX* mdctx, QString& outHex)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        return false;
    }
    outHex = digestToHex(hash, hashLen);
    return true;
}

void writeCheckpoint(const Options& options, const QString& algoName, uint64_t deviceSize,
                     uint64_t blockSize, const QStringList& blockHashes, uint64_t bytesDone)
{
    if (!options.checkpointOut) {
        return;
    }
    options.checkpointOut->deviceNode = options.deviceNode;
    options.checkpointOut->algorithm = algoName;
    options.checkpointOut->scanMode = scanModeTag(options.scanMode);
    options.checkpointOut->deviceSize = deviceSize;
    options.checkpointOut->blockSize = blockSize;
    options.checkpointOut->blockHashes = blockHashes;
    options.checkpointOut->bytesCompleted = bytesDone;
}

HashResult hashQuickSampleWin(HANDLE handle, const Options& options, uint64_t deviceSize)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm) + QStringLiteral("-QUICK");

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx || EVP_DigestInit_ex(mdctx, mdFor(options.algorithm), nullptr) != 1) {
        if (mdctx) {
            EVP_MD_CTX_free(mdctx);
        }
        result.errorMessage = QStringLiteral("Failed to init hash");
        return result;
    }

    const uint64_t sampleSize = qMin<uint64_t>(1024ULL * 1024, deviceSize);
    QVector<uint64_t> offsets;
    offsets.append(0);
    if (deviceSize > sampleSize) {
        offsets.append(deviceSize - sampleSize);
    }
    const int spaced = 14;
    for (int i = 1; i < spaced; ++i) {
        uint64_t off = (deviceSize * static_cast<uint64_t>(i)) / static_cast<uint64_t>(spaced);
        if (off + sampleSize > deviceSize) {
            off = deviceSize > sampleSize ? deviceSize - sampleSize : 0;
        }
        if (!offsets.contains(off)) {
            offsets.append(off);
        }
    }

    QByteArray buffer;
    buffer.resize(static_cast<int>(sampleSize));
    uint64_t processed = 0;

    for (uint64_t off : offsets) {
        if (cancelled(options)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Cancelled");
            return result;
        }
        if (!winSeek(handle, off)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage =
                QStringLiteral("Seek failed: Win32 error %1").arg(GetLastError());
            return result;
        }
        const size_t toRead = static_cast<size_t>(qMin(sampleSize, deviceSize - off));
        size_t got = 0;
        if (!winRead(handle, buffer.data(), toRead, &got)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Read error: Win32 error %1").arg(GetLastError());
            return result;
        }
        if (EVP_DigestUpdate(mdctx, buffer.constData(), got) != 1) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Hash update failed");
            return result;
        }
        processed += got;
        reportProgress(options, processed);
    }

    QString outHex;
    if (!finalizeCtx(mdctx, outHex)) {
        EVP_MD_CTX_free(mdctx);
        result.errorMessage = QStringLiteral("Finalize failed");
        return result;
    }
    EVP_MD_CTX_free(mdctx);
    result.hash = outHex;
    result.bytesProcessed = processed;
    result.success = true;
    return result;
}

HashResult hashChunkedResumeWin(HANDLE handle, const Options& options, uint64_t deviceSize)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    const uint64_t blockSize = kDefaultChunkBytes;
    const uint64_t numBlocks = (deviceSize + blockSize - 1) / blockSize;

    QStringList blockHashes;
    uint64_t startBlock = 0;
    uint64_t bytesDone = 0;
    if (options.checkpointOut && options.checkpointOut->isValid()
        && options.checkpointOut->blockSize == blockSize) {
        blockHashes = options.checkpointOut->blockHashes;
        startBlock = static_cast<uint64_t>(blockHashes.size());
        bytesDone = options.checkpointOut->bytesCompleted;
        if (options.resumeFromBytes > 0) {
            startBlock = options.resumeFromBytes / blockSize;
            while (static_cast<uint64_t>(blockHashes.size()) > startBlock) {
                blockHashes.removeLast();
            }
            bytesDone = startBlock * blockSize;
        }
    }

    const size_t bufSize = static_cast<size_t>(normalizedBufferSizeKB(options.bufferSizeKB)) * 1024;
    QByteArray buffer;
    buffer.resize(static_cast<int>(bufSize));

    for (uint64_t block = startBlock; block < numBlocks; ++block) {
        if (cancelled(options)) {
            writeCheckpoint(options, result.algorithm, deviceSize, blockSize, blockHashes, bytesDone);
            result.errorMessage = QStringLiteral("Cancelled");
            return result;
        }

        const uint64_t offset = block * blockSize;
        const uint64_t chunkLen = qMin(blockSize, deviceSize - offset);
        if (!winSeek(handle, offset)) {
            result.errorMessage =
                QStringLiteral("Seek failed: Win32 error %1").arg(GetLastError());
            return result;
        }

        EVP_MD_CTX* blockCtx = EVP_MD_CTX_new();
        if (!blockCtx || EVP_DigestInit_ex(blockCtx, mdFor(options.algorithm), nullptr) != 1) {
            if (blockCtx) {
                EVP_MD_CTX_free(blockCtx);
            }
            result.errorMessage = QStringLiteral("Block hash init failed");
            return result;
        }

        uint64_t readInBlock = 0;
        while (readInBlock < chunkLen) {
            if (cancelled(options)) {
                EVP_MD_CTX_free(blockCtx);
                writeCheckpoint(options, result.algorithm, deviceSize, blockSize, blockHashes,
                              bytesDone);
                result.errorMessage = QStringLiteral("Cancelled");
                return result;
            }
            const size_t toRead =
                static_cast<size_t>(qMin<uint64_t>(bufSize, chunkLen - readInBlock));
            size_t n = 0;
            if (!winRead(handle, buffer.data(), toRead, &n)) {
                EVP_MD_CTX_free(blockCtx);
                result.errorMessage = QStringLiteral("Read error: Win32 error %1")
                                          .arg(GetLastError());
                return result;
            }
            EVP_DigestUpdate(blockCtx, buffer.constData(), n);
            readInBlock += n;
            bytesDone += n;
            reportProgress(options, bytesDone);
        }

        QString blockHex;
        if (!finalizeCtx(blockCtx, blockHex)) {
            EVP_MD_CTX_free(blockCtx);
            result.errorMessage = QStringLiteral("Block finalize failed");
            return result;
        }
        EVP_MD_CTX_free(blockCtx);
        blockHashes.append(blockHex);

        const int every = qMax(1, options.checkpointEveryBlocks);
        if (options.checkpointOut
            && ((blockHashes.size() % every) == 0 || block + 1 == numBlocks)) {
            writeCheckpoint(options, result.algorithm, deviceSize, blockSize, blockHashes, bytesDone);
        }
    }

    result.hash = combineBlockHashes(blockHashes, options.algorithm);
    result.bytesProcessed = bytesDone;
    result.success = !result.hash.isEmpty();
    if (!result.success && result.errorMessage.isEmpty()) {
        result.errorMessage = QStringLiteral("Failed to combine block hashes");
    }
    return result;
}

} // namespace

HashResult hashAdvanced(int fd, const Options& options, uint64_t deviceSize)
{
    HANDLE handle = handleFromFd(fd);
    if (options.scanMode == ScanMode::QuickSample) {
        return hashQuickSampleWin(handle, options, deviceSize);
    }
    return hashChunkedResumeWin(handle, options, deviceSize);
}

} // namespace FlashSpartan::RawDeviceHash

#else

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace FlashSpartan::RawDeviceHash {

namespace {

const EVP_MD* mdFor(Algorithm algo)
{
    switch (algo) {
        case Algorithm::SHA512: return EVP_sha512();
        case Algorithm::BLAKE2b: return EVP_blake2b512();
        case Algorithm::SHA256:
        default: return EVP_sha256();
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

QString digestToHex(const unsigned char* hash, unsigned int hashLen)
{
    QByteArray hashHex;
    hashHex.reserve(static_cast<int>(hashLen * 2));
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QString("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }
    return QString::fromLatin1(hashHex);
}

bool finalizeCtx(EVP_MD_CTX* mdctx, QString& outHex)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        return false;
    }
    outHex = digestToHex(hash, hashLen);
    return true;
}

void writeCheckpoint(const Options& options, const QString& algoName,
                     uint64_t deviceSize, uint64_t blockSize, const QStringList& blockHashes,
                     uint64_t bytesDone)
{
    if (!options.checkpointOut) {
        return;
    }
    options.checkpointOut->deviceNode = options.deviceNode;
    options.checkpointOut->algorithm = algoName;
    options.checkpointOut->scanMode = scanModeTag(options.scanMode);
    options.checkpointOut->deviceSize = deviceSize;
    options.checkpointOut->blockSize = blockSize;
    options.checkpointOut->blockHashes = blockHashes;
    options.checkpointOut->bytesCompleted = bytesDone;
}

HashResult hashQuickSample(int fd, const Options& options, uint64_t deviceSize)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm) + QStringLiteral("-QUICK");

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx || EVP_DigestInit_ex(mdctx, mdFor(options.algorithm), nullptr) != 1) {
        result.errorMessage = QStringLiteral("Failed to init hash");
        if (mdctx) {
            EVP_MD_CTX_free(mdctx);
        }
        return result;
    }

    const uint64_t sampleSize = qMin<uint64_t>(1024ULL * 1024, deviceSize);
    QVector<uint64_t> offsets;
    offsets.append(0);
    if (deviceSize > sampleSize) {
        offsets.append(deviceSize - sampleSize);
    }
    const int spaced = 14;
    for (int i = 1; i < spaced; ++i) {
        uint64_t off = (deviceSize * static_cast<uint64_t>(i)) / static_cast<uint64_t>(spaced);
        if (off + sampleSize > deviceSize) {
            off = deviceSize > sampleSize ? deviceSize - sampleSize : 0;
        }
        if (!offsets.contains(off)) {
            offsets.append(off);
        }
    }

    QByteArray buffer;
    buffer.resize(static_cast<int>(sampleSize));
    uint64_t processed = 0;

    for (uint64_t off : offsets) {
        if (cancelled(options)) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QStringLiteral("Cancelled");
            return result;
        }
        if (lseek(fd, static_cast<off_t>(off), SEEK_SET) < 0) {
            EVP_MD_CTX_free(mdctx);
            result.errorMessage = QString("Seek failed: %1").arg(strerror(errno));
            return result;
        }
        const size_t toRead = static_cast<size_t>(qMin(sampleSize, deviceSize - off));
        size_t got = 0;
        while (got < toRead) {
            const ssize_t n = read(fd, buffer.data() + got, toRead - got);
            if (n <= 0) {
                EVP_MD_CTX_free(mdctx);
                result.errorMessage = n < 0 ? QString("Read error: %1").arg(strerror(errno))
                                            : QStringLiteral("Unexpected EOF");
                return result;
            }
            got += static_cast<size_t>(n);
        }
        EVP_DigestUpdate(mdctx, buffer.constData(), got);
        processed += got;
        reportProgress(options, processed);
    }

    if (!finalizeCtx(mdctx, result.hash)) {
        result.errorMessage = QStringLiteral("Failed to finalize quick hash");
    } else {
        result.bytesProcessed = processed;
        result.success = true;
    }
    EVP_MD_CTX_free(mdctx);
    return result;
}

HashResult hashChunkedResume(int fd, const Options& options, uint64_t deviceSize)
{
    HashResult result;
    result.deviceNode = options.deviceNode;
    result.algorithm = algorithmName(options.algorithm);

    const uint64_t blockSize = kDefaultChunkBytes;
    const uint64_t numBlocks = (deviceSize + blockSize - 1) / blockSize;

    QStringList blockHashes;
    uint64_t startBlock = 0;
    uint64_t bytesDone = 0;

    if (options.checkpointOut && options.checkpointOut->isValid()
        && options.checkpointOut->deviceSize == deviceSize
        && options.checkpointOut->blockSize == blockSize) {
        blockHashes = options.checkpointOut->blockHashes;
        startBlock = static_cast<uint64_t>(blockHashes.size());
        bytesDone = options.checkpointOut->bytesCompleted;
        if (options.resumeFromBytes > 0) {
            startBlock = options.resumeFromBytes / blockSize;
            while (static_cast<uint64_t>(blockHashes.size()) > startBlock) {
                blockHashes.removeLast();
            }
            bytesDone = startBlock * blockSize;
        }
    }

    const size_t bufSize = static_cast<size_t>(options.bufferSizeKB) * 1024;
    QByteArray buffer;
    buffer.resize(static_cast<int>(bufSize));

    for (uint64_t block = startBlock; block < numBlocks; ++block) {
        if (cancelled(options)) {
            writeCheckpoint(options, result.algorithm, deviceSize, blockSize, blockHashes, bytesDone);
            result.errorMessage = QStringLiteral("Cancelled");
            return result;
        }

        const uint64_t offset = block * blockSize;
        const uint64_t chunkLen = qMin(blockSize, deviceSize - offset);
        if (lseek(fd, static_cast<off_t>(offset), SEEK_SET) < 0) {
            result.errorMessage = QString("Seek failed: %1").arg(strerror(errno));
            return result;
        }

        EVP_MD_CTX* blockCtx = EVP_MD_CTX_new();
        if (!blockCtx || EVP_DigestInit_ex(blockCtx, mdFor(options.algorithm), nullptr) != 1) {
            if (blockCtx) {
                EVP_MD_CTX_free(blockCtx);
            }
            result.errorMessage = QStringLiteral("Block hash init failed");
            return result;
        }

        uint64_t readInBlock = 0;
        while (readInBlock < chunkLen) {
            if (cancelled(options)) {
                EVP_MD_CTX_free(blockCtx);
                writeCheckpoint(options, result.algorithm, deviceSize, blockSize, blockHashes,
                              bytesDone);
                result.errorMessage = QStringLiteral("Cancelled");
                return result;
            }
            const size_t toRead = static_cast<size_t>(qMin<uint64_t>(bufSize, chunkLen - readInBlock));
            const ssize_t n = read(fd, buffer.data(), toRead);
            if (n <= 0) {
                EVP_MD_CTX_free(blockCtx);
                result.errorMessage = n < 0 ? QString("Read error: %1").arg(strerror(errno))
                                            : QStringLiteral("Unexpected EOF");
                return result;
            }
            EVP_DigestUpdate(blockCtx, buffer.constData(), static_cast<size_t>(n));
            readInBlock += static_cast<uint64_t>(n);
            bytesDone += static_cast<uint64_t>(n);
            reportProgress(options, bytesDone);
        }

        QString blockHex;
        if (!finalizeCtx(blockCtx, blockHex)) {
            EVP_MD_CTX_free(blockCtx);
            result.errorMessage = QStringLiteral("Block finalize failed");
            return result;
        }
        EVP_MD_CTX_free(blockCtx);
        blockHashes.append(blockHex);

        const int every = qMax(1, options.checkpointEveryBlocks);
        if (options.checkpointOut
            && ((blockHashes.size() % every) == 0 || block + 1 == numBlocks)) {
            writeCheckpoint(options, result.algorithm, deviceSize, blockSize, blockHashes, bytesDone);
        }
    }

    result.hash = combineBlockHashes(blockHashes, options.algorithm);
    result.bytesProcessed = bytesDone;
    result.success = !result.hash.isEmpty();
    if (!result.success && result.errorMessage.isEmpty()) {
        result.errorMessage = QStringLiteral("Failed to combine block hashes");
    }
    return result;
}

} // namespace

QString scanModeTag(ScanMode mode)
{
    return mode == ScanMode::QuickSample ? QStringLiteral("quick") : QStringLiteral("full");
}

QString combineBlockHashes(const QStringList& blockHex, Algorithm algo)
{
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx || EVP_DigestInit_ex(mdctx, mdFor(algo), nullptr) != 1) {
        if (mdctx) {
            EVP_MD_CTX_free(mdctx);
        }
        return {};
    }
    for (const QString& hex : blockHex) {
        const QByteArray raw = QByteArray::fromHex(hex.toLatin1());
        EVP_DigestUpdate(mdctx, raw.constData(), static_cast<size_t>(raw.size()));
    }
    QString out;
    if (!finalizeCtx(mdctx, out)) {
        out.clear();
    }
    EVP_MD_CTX_free(mdctx);
    return out;
}

HashResult hashAdvanced(int fd, const Options& options, uint64_t deviceSize)
{
    if (options.scanMode == ScanMode::QuickSample) {
        return hashQuickSample(fd, options, deviceSize);
    }
    return hashChunkedResume(fd, options, deviceSize);
}

} // namespace FlashSpartan::RawDeviceHash

#endif
