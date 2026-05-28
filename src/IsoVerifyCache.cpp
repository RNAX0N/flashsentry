#include "IsoVerifyCache.h"

#include <QHash>
#include <QMutex>
#include <QString>

namespace FlashSentry {

namespace {

struct CacheKey {
    QString path;
    qint64 size = 0;
    qint64 mtimeMs = 0;

    bool operator==(const CacheKey& o) const
    {
        return path == o.path && size == o.size && mtimeMs == o.mtimeMs;
    }
};

size_t qHash(const CacheKey& k, size_t seed = 0)
{
    return qHash(k.path, seed) ^ static_cast<size_t>(k.size) ^ static_cast<size_t>(k.mtimeMs);
}

QMutex g_mutex;
QHash<CacheKey, QString> g_cache;

} // namespace

QString IsoVerifyCache::lookup(const QString& filePath, qint64 size, qint64 mtimeMs)
{
    const CacheKey key{filePath, size, mtimeMs};
    QMutexLocker lock(&g_mutex);
    return g_cache.value(key);
}

void IsoVerifyCache::store(const QString& filePath, qint64 size, qint64 mtimeMs,
                           const QString& sha256Hex)
{
    const CacheKey key{filePath, size, mtimeMs};
    QMutexLocker lock(&g_mutex);
    g_cache.insert(key, sha256Hex.trimmed().toLower());
}

void IsoVerifyCache::clear()
{
    QMutexLocker lock(&g_mutex);
    g_cache.clear();
}

} // namespace FlashSentry
