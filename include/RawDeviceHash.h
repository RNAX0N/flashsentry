#pragma once

#include "Types.h"

#include <QString>
#include <atomic>
#include <cstdint>

namespace FlashSentry::RawDeviceHash {

enum class Algorithm {
    SHA256,
    SHA512,
    BLAKE2b,
};

inline constexpr int kMinBufferSizeKB = 64;
inline constexpr int kDefaultBufferSizeKB = 1024;
inline constexpr int kMaxBufferSizeKB = 16 * 1024;

QString algorithmName(Algorithm algo);
Algorithm algorithmFromName(const QString& name);
int normalizedBufferSizeKB(int requestedKB);

struct Options {
    QString deviceNode;
    Algorithm algorithm = Algorithm::SHA256;
    int bufferSizeKB = kDefaultBufferSizeKB;
    bool useMemoryMapping = true;
    std::atomic<bool>* cancelled = nullptr;
    std::atomic<uint64_t>* bytesProcessed = nullptr;
};

/** Validate and open a /dev block device read-only. Returns fd or -1 and sets errno. */
int openDevice(const QString& deviceNode);

uint64_t deviceSize(int fd, const QString& deviceNode);

/** Hash using an already-open fd. Caller closes fd. */
HashResult hashOpenFd(int fd, const Options& options);

/** Open (or use polkit helper) and hash. */
HashResult hashDevice(const Options& options, const QString& pkexecHelperPath = QString());

} // namespace FlashSentry::RawDeviceHash
