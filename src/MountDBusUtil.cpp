#include "MountDBusUtil.h"

namespace FlashSpartan {

QString MountDBusUtil::formatMountError(const QString& dbusMessage)
{
    if (dbusMessage.contains(QStringLiteral("NotAuthorized"))) {
        return QStringLiteral("Permission denied. You may need to authenticate.");
    }
    if (dbusMessage.contains(QStringLiteral("AlreadyMounted"))) {
        return QStringLiteral("Device is already mounted.");
    }
    if (dbusMessage.contains(QStringLiteral("NotMounted"))) {
        return QStringLiteral("Device is not mounted.");
    }
    if (dbusMessage.contains(QStringLiteral("Busy"))) {
        return QStringLiteral(
            "Device is busy. Please close any open files or applications using this device.");
    }
    if (dbusMessage.contains(QStringLiteral("NoFilesystem"))) {
        return QStringLiteral("No recognizable filesystem found on device.");
    }
    return dbusMessage;
}

} // namespace FlashSpartan
