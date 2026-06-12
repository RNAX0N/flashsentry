#pragma once

#include <QString>

namespace FlashSpartan {

/**
 * Sparse file fingerprint: SHA-256 of file size plus first and last 1 MiB.
 * Used for fast tamper detection before a full image hash.
 */
class IsoQuickFingerprint {
public:
    static QString compute(const QString& filePath, QString* errorOut = nullptr);
};

} // namespace FlashSpartan
