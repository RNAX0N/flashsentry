#pragma once

#include "DeviceDriveUtil.h"
#include "Types.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace FlashSpartan {

class DatabaseManager;

/** Pure trust-flow decisions and whitelist mutations (no UI). */
class DeviceTrustCoordinator {
public:
    enum class NewDeviceAction {
        DriveBlocked,
        SkipDuplicatePrompt,
        WhitelistPartition,
        PromptPartition,
        WhitelistDrive,
        PromptDrive,
    };

    struct NewDevicePlan {
        NewDeviceAction action = NewDeviceAction::PromptPartition;
        QString driveKey;
    };

    struct WhitelistResult {
        QStringList whitelistedDeviceNodes;
        int recordsAdded = 0;
    };

    static NewDevicePlan planNewDevice(const DeviceInfo& device, bool promptPerPartition,
                                      bool requireConfirmation, bool drivePromptInProgress,
                                      const QList<DeviceInfo>& connected,
                                      const DatabaseManager& database);

    static WhitelistResult whitelistDrivePartitions(const QString& driveKey,
                                                   const QList<DeviceInfo>& connected,
                                                   DatabaseManager& database, int trustLevel);
};

} // namespace FlashSpartan
