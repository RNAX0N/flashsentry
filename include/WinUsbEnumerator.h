#pragma once

#include "UsbHostTier.h"

#include <QString>
#include <QStringList>

namespace FlashSpartan {

struct UsbHostDeviceInfo {
    QString instanceId;
    QString displayName;
    QString category;
    QString manufacturer;
    QString vendorId;
    QString productId;
    UsbHostTier tier = UsbHostTier::InternalHost;

    QString stableKey() const { return instanceId; }
    bool isInternalHost() const { return tier == UsbHostTier::InternalHost; }
};

namespace WinUsbEnumerator {

#ifdef Q_OS_WIN
/** All present USB\ host nodes with tier classification (internal vs peripheral). */
QList<UsbHostDeviceInfo> enumeratePresentUsbDevices();
#endif

} // namespace WinUsbEnumerator
} // namespace FlashSpartan
