#include "BlockedDriveStore.h"

#include "policy/PolicyGateway.h"
#include "policy/PolicyServiceLocator.h"

namespace FlashSentry {

BlockedDriveStore& BlockedDriveStore::instance()
{
    static BlockedDriveStore inst;
    return inst;
}

void BlockedDriveStore::refreshFromGateway()
{
    m_entries.clear();
    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    if (!gate) {
        return;
    }
    for (const BlockedDriveEntry& e : gate->snapshot().blocks) {
        m_entries.append(e);
    }
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
    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    if (!gate) {
        return;
    }
    gate->blockDrive(driveKey, uniqueId, label, QStringLiteral("ui"));
    refreshFromGateway();
}

void BlockedDriveStore::unblock(const QString& driveKey, const QString& uniqueId)
{
    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    if (!gate) {
        return;
    }
    gate->unblockDrive(driveKey, uniqueId, QStringLiteral("ui"));
    refreshFromGateway();
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
