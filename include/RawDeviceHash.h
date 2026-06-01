#pragma once

#include "Types.h"

#include <QString>
#include <atomic>
#include <cstdint>

namespace FlashSpartan { struct HashCheckpoint; }

namespace FlashSpartan::RawDeviceHash {

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

enum class ScanMode {
    Full,
    QuickSample,
};

struct Options {
    QString deviceNode;
    Algorithm algorithm = Algorithm::SHA256;
    int bufferSizeKB = kDefaultBufferSizeKB;
    bool useMemoryMapping = true;
    std::atomic<bool>* cancelled = nullptr;
    std::atomic<uint64_t>* bytesProcessed = nullptr;
    ScanMode scanMode = ScanMode::Full;
    uint64_t resumeFromBytes = 0;
    FlashSpartan::HashCheckpoint* checkpointOut = nullptr;
    int checkpointEveryBlocks = 4;
};

static constexpr uint64_t kDefaultChunkBytes = 64ULL * 1024 * 1024;


/** Open block device read-only (direct open only). Returns fd or -1. */
int openDevice(const QString& deviceNode);

uint64_t deviceSize(int fd, const QString& deviceNode);

/** Close a device fd from openDevice(); ignores fd < 0. */
void closeDevice(int fd);

/** Hash using an already-open fd. Caller closes fd. */
HashResult hashOpenFd(int fd, const Options& options);

/** Open (or use polkit helper) and hash. */
HashResult hashDevice(const Options& options, const QString& pkexecHelperPath = QString());

} // namespace FlashSpartan::RawDeviceHash
