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

/** Host nodes worth showing in USB Monitor (excludes hubs, composite interfaces, generic controllers). */
bool isUserVisibleInUsbMonitor(const UsbHostDeviceInfo& info);
#endif

} // namespace WinUsbEnumerator
} // namespace FlashSpartan
