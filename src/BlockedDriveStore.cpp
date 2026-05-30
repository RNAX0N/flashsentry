#include "BlockedDriveStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace FlashSentry {

namespace {

QString storePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/FlashSentry/blocked-drives.json");
}

} // namespace

BlockedDriveStore& BlockedDriveStore::instance()
{
    static BlockedDriveStore inst;
    return inst;
}

void BlockedDriveStore::load()
{
    m_entries.clear();
    QFile f(storePath());
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonArray arr = doc.object()[QStringLiteral("entries")].toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        BlockedDriveEntry e;
        e.driveKey = o[QStringLiteral("drive_key")].toString();
        e.uniqueId = o[QStringLiteral("unique_id")].toString();
        e.label = o[QStringLiteral("label")].toString();
        e.blockedAt = QDateTime::fromString(o[QStringLiteral("blocked_at")].toString(), Qt::ISODate);
        if (!e.driveKey.isEmpty() || !e.uniqueId.isEmpty()) {
            m_entries.append(e);
        }
    }
}

void BlockedDriveStore::save()
{
    const QString path = storePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const BlockedDriveEntry& e : m_entries) {
        QJsonObject o;
        o[QStringLiteral("drive_key")] = e.driveKey;
        o[QStringLiteral("unique_id")] = e.uniqueId;
        o[QStringLiteral("label")] = e.label;
        o[QStringLiteral("blocked_at")] = e.blockedAt.toString(Qt::ISODate);
        arr.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("entries")] = arr;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool BlockedDriveStore::isBlocked(const QString& driveKey, const QString& uniqueId) const
{
    for (const BlockedDriveEntry& e : m_entries) {
        if (!driveKey.isEmpty() && e.driveKey == driveKey) {
            return true;
        }
        if (!uniqueId.isEmpty() && e.uniqueId == uniqueId) {
            return true;
        }
    }
    return false;
}

void BlockedDriveStore::block(const QString& driveKey, const QString& uniqueId, const QString& label)
{
    if (isBlocked(driveKey, uniqueId)) {
        return;
    }
    BlockedDriveEntry e;
    e.driveKey = driveKey;
    e.uniqueId = uniqueId;
    e.label = label;
    e.blockedAt = QDateTime::currentDateTime();
    m_entries.append(e);
    save();
}

void BlockedDriveStore::unblock(const QString& driveKey, const QString& uniqueId)
{
    QList<BlockedDriveEntry> kept;
    for (const BlockedDriveEntry& e : m_entries) {
        const bool matchKey = !driveKey.isEmpty() && e.driveKey == driveKey;
        const bool matchId = !uniqueId.isEmpty() && e.uniqueId == uniqueId;
        if (matchKey || matchId) {
            continue;
        }
        kept.append(e);
    }
    if (kept.size() != m_entries.size()) {
        m_entries = kept;
        save();
    }
}

QList<BlockedDriveEntry> BlockedDriveStore::entries() const
{
    return m_entries;
}

QSet<QString> BlockedDriveStore::blockedDriveKeys() const
{
    QSet<QString> keys;
    for (const BlockedDriveEntry& e : m_entries) {
        if (!e.driveKey.isEmpty()) {
            keys.insert(e.driveKey);
        }
    }
    return keys;
}

QSet<QString> BlockedDriveStore::blockedUniqueIds() const
{
    QSet<QString> ids;
    for (const BlockedDriveEntry& e : m_entries) {
        if (!e.uniqueId.isEmpty()) {
            ids.insert(e.uniqueId);
        }
    }
    return ids;
}

} // namespace FlashSentry
