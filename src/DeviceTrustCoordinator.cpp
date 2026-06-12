#include "DeviceTrustCoordinator.h"

#include "DatabaseManager.h"
#include "DeviceWhitelistService.h"

namespace FlashSpartan {

DeviceTrustCoordinator::NewDevicePlan DeviceTrustCoordinator::planNewDevice(
    const DeviceInfo& device, bool promptPerPartition, bool requireConfirmation,
    bool drivePromptInProgress, const QList<DeviceInfo>& connected,
    const DatabaseManager& database)
{
    NewDevicePlan plan;
    plan.driveKey = DeviceDriveUtil::driveKey(device);

    if (DeviceDriveUtil::isDriveBlocked(device, database)) {
        plan.action = NewDeviceAction::DriveBlocked;
        return plan;
    }

    if (promptPerPartition) {
        plan.action = requireConfirmation ? NewDeviceAction::PromptPartition
                                          : NewDeviceAction::WhitelistPartition;
        return plan;
    }

    if (DeviceDriveUtil::isDriveKnown(device, connected, database)) {
        plan.action = requireConfirmation ? NewDeviceAction::PromptPartition
                                          : NewDeviceAction::WhitelistPartition;
        return plan;
    }

    if (drivePromptInProgress) {
        plan.action = NewDeviceAction::SkipDuplicatePrompt;
        return plan;
    }

    plan.action = requireConfirmation ? NewDeviceAction::PromptDrive : NewDeviceAction::WhitelistDrive;
    return plan;
}

DeviceTrustCoordinator::WhitelistResult DeviceTrustCoordinator::whitelistDrivePartitions(
    const QString& driveKey, const QList<DeviceInfo>& connected, DatabaseManager& database,
    int trustLevel)
{
    WhitelistResult result;
    for (const DeviceInfo& part : DeviceDriveUtil::partitionsOnDrive(driveKey, connected)) {
        if (database.hasDevice(part)) {
            continue;
        }
        const DeviceRecord record = DeviceWhitelistService::makeRecord(part, database, trustLevel);
        if (database.addDevice(record)) {
            ++result.recordsAdded;
            result.whitelistedDeviceNodes.append(part.deviceNode);
        }
    }
    return result;
}

} // namespace FlashSpartan
