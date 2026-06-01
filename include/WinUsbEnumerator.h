#pragma once

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

    QString stableKey() const { return instanceId; }
};

namespace WinUsbEnumerator {

#ifdef Q_OS_WIN
QList<UsbHostDeviceInfo> enumeratePresentUsbDevices();
#endif

} // namespace WinUsbEnumerator
} // namespace FlashSpartan
