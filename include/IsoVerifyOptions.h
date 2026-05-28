#pragma once

#include <atomic>
#include <functional>

#include <QString>

namespace FlashSentry {

struct IsoVerifyOptions {
    bool useHashCache = true;
    int maxParallel = 2;
    /** Hash decompressed stream for .img.xz (requires xz in PATH). */
    bool verifyDecompressed = false;
    bool preferOfflineSidecars = false;
    std::atomic<bool>* cancelled = nullptr;
    std::function<void(int current, int total, const QString& fileName)> progress;
};

} // namespace FlashSentry
