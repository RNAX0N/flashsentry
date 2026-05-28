#pragma once

#include "HashCheckpoint.h"
#include "RawDeviceHash.h"

namespace FlashSentry::RawDeviceHash {

QString scanModeTag(ScanMode mode);

QString combineBlockHashes(const QStringList& blockHex, Algorithm algo);

/** Quick sample or resumable chunked full read. */
HashResult hashAdvanced(int fd, const Options& options, uint64_t deviceSize);

} // namespace FlashSentry::RawDeviceHash
