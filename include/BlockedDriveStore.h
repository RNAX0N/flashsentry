#pragma once

#include <QDateTime>
#include <QList>
#include <QSet>
#include <QString>

namespace FlashSentry {

struct BlockedDriveEntry {
    QString driveKey;
    QString uniqueId;
    QString label;
    QDateTime blockedAt;
};

/** Persisted block list (survives app restart). */
class BlockedDriveStore {
public:
    static BlockedDriveStore& instance();

    void load();
    void save();

    bool isBlocked(const QString& driveKey, const QString& uniqueId = {}) const;

    void block(const QString& driveKey, const QString& uniqueId, const QString& label = {});
    void unblock(const QString& driveKey, const QString& uniqueId = {});

    QList<BlockedDriveEntry> entries() const;
    QSet<QString> blockedDriveKeys() const;
    QSet<QString> blockedUniqueIds() const;

private:
    BlockedDriveStore() = default;

    QList<BlockedDriveEntry> m_entries;
};

} // namespace FlashSentry
