#pragma once

#include "PolicySnapshot.h"

#include "Types.h"

#include <QReadWriteLock>
#include <QString>

namespace FlashSentry::Policy {

/**
 * Private in-process policy store (custom signed blob).
 * Only PolicyGateway / policyd should use this class.
 */
class PolicyStoreEngine {
public:
  explicit PolicyStoreEngine(QString storePath = {});

    bool load(QString* error = nullptr);
    bool save(QString* error = nullptr);

    PolicySnapshot snapshot() const;
    void setSnapshot(const PolicySnapshot& snap);

    bool migrateLegacyJsonIfNeeded(QString* error = nullptr);

    // Mutations (audit + save)
    bool upsertDevice(const DeviceRecord& record, const QString& actor, const QString& reason);
    bool removeDevice(const QString& uniqueId, const QString& actor, const QString& reason);
    bool clearDevices(const QString& actor, const QString& reason);
    bool blockDrive(const QString& driveKey, const QString& uniqueId, const QString& label,
                    const QString& actor);
    bool unblockDrive(const QString& driveKey, const QString& uniqueId, const QString& actor);

private:
    bool commitMutation(const QString& actor, const QString& action, const QString& target,
                        const QString& detail);

    QString m_storePath;
    PolicySnapshot m_snapshot;
    mutable QReadWriteLock m_lock;
};

} // namespace FlashSentry::Policy
