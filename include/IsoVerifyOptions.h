#pragma once

#include "Types.h"

#include <QHash>
#include <atomic>
#include <functional>

#include <QString>

namespace FlashSpartan {

struct IsoVerifyOptions {
    bool useHashCache = true;
    int maxParallel = 2;
    /** Hash decompressed stream for .img.xz (requires xz in PATH). */
    bool verifyDecompressed = false;
    bool preferOfflineSidecars = false;
    /** Relative path → baseline recorded on this USB stick (when re-verifying). */
    QHash<QString, IsoImageBaseline> stickBaselines;
    bool quickFingerprintFirst = false;
    std::atomic<bool>* cancelled = nullptr;
    std::function<void(int current, int total, const QString& fileName)> progress;
};

} // namespace FlashSpartan
