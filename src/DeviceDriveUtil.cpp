#include "DeviceDriveUtil.h"

#include "BlockedDriveStore.h"
#include "DatabaseManager.h"

#include <QDir>

#include <QtGlobal>

namespace FlashSpartan {

QString DeviceDriveUtil::driveKey(const DeviceInfo& device)
{
    if (!device.parentDevice.isEmpty()) {
        return device.parentDevice;
    }
#ifdef Q_OS_WIN
    return QDir::toNativeSeparators(device.deviceNode).trimmed().toUpper();
#else
    return device.deviceNode.section(QLatin1Char('/'), -1);
#endif
}

QList<DeviceInfo> DeviceDriveUtil::partitionsOnDrive(const QString& drive,
                                                     const QList<DeviceInfo>& connected)
{
    QList<DeviceInfo> parts;
    for (const DeviceInfo& part : connected) {
        if (DeviceDriveUtil::driveKey(part) == drive) {
            parts.append(part);
        }
    }
    return parts;
}

bool DeviceDriveUtil::isDriveKnown(const DeviceInfo& device, const QList<DeviceInfo>& connected,
                                   const DatabaseManager& database)
{
    const QString drive = driveKey(device);
    for (const DeviceInfo& part : connected) {
        if (driveKey(part) != drive) {
            continue;
        }
        if (database.hasDevice(part)) {
            return true;
        }
    }
    return false;
}

bool DeviceDriveUtil::isDriveBlocked(const DeviceInfo& device, const DatabaseManager& database)
{
    const QString key = driveKey(device);
    const QString uid = database.canonicalUniqueId(device);
    return BlockedDriveStore::instance().isBlocked(key, uid);
}

DeviceDriveUtil::DriveSummary DeviceDriveUtil::summarizeDrive(const DeviceInfo& device,
                                                              const QList<DeviceInfo>& connected)
{
    DriveSummary summary;
    summary.driveKey = driveKey(device);
    for (const DeviceInfo& part : partitionsOnDrive(summary.driveKey, connected)) {
        ++summary.partitionCount;
        summary.partitionNodes.append(part.deviceNode);
    }
    return summary;
}

} // namespace FlashSpartan
