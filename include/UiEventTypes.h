#pragma once

#include <QDateTime>
#include <QString>

namespace FlashSpartan {

/** Row in the USB Monitor "Recent events" table. */
struct UiEventEntry {
    QString id;
    QDateTime time;
    QString event;
    QString device;
    QString type;
    QString result;
    QString detail;
    QString deviceNode;
};

/** Row in the USB Monitor connected-devices table. */
struct UsbDeviceRow {
    QString deviceNode;
    QString displayName;
    QString type;
    QString status;
    QString capacity;
    QString vendorModel;
    QString connectedAt;
    QString disconnectedAt;
    bool isConnected = true;
    bool nameEditable = true;
};

struct UsbMonitorStats {
    int connected = 0;
    int allowed = 0;
    int blocked = 0;
    int events = 0;
};

} // namespace FlashSpartan
