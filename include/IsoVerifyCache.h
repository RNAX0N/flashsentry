#pragma once

#include <QString>

namespace FlashSentry {

/** Session-persistent SHA-256 cache keyed by path, size, and mtime. */
class IsoVerifyCache {
public:
    static QString lookup(const QString& filePath, qint64 size, qint64 mtimeMs);
    static void store(const QString& filePath, qint64 size, qint64 mtimeMs, const QString& sha256Hex);

    static void clear();
};

} // namespace FlashSentry
