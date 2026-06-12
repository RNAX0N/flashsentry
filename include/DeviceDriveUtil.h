#pragma once

#include "Types.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace FlashSpartan {

class DatabaseManager;

/** Physical-drive grouping and block-list lookups for USB trust flows. */
class DeviceDriveUtil {
public:
    static QString driveKey(const DeviceInfo& device);

    static QList<DeviceInfo> partitionsOnDrive(const QString& driveKey,
                                               const QList<DeviceInfo>& connected);

    static bool isDriveKnown(const DeviceInfo& device, const QList<DeviceInfo>& connected,
                             const DatabaseManager& database);

    static bool isDriveBlocked(const DeviceInfo& device, const DatabaseManager& database);

    struct DriveSummary {
        QString driveKey;
        int partitionCount = 0;
        QStringList partitionNodes;
    };

    static DriveSummary summarizeDrive(const DeviceInfo& device,
                                       const QList<DeviceInfo>& connected);
};

} // namespace FlashSpartan
