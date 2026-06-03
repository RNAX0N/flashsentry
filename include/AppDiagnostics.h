#pragma once

#include "UsbHostTier.h"
#include "WinUsbEnumerator.h"

#include <QString>

namespace FlashSpartan {

/** File paths and diagnostic exports (logs, USB inventory snapshots). */
class AppDiagnostics {
public:
    static QString logsDir();
    static QString qtLogPath();
    static QString hostUsbInventoryPath();

    /** Append one JSON line per host node (all tiers) for support / triage. */
    static void appendHostUsbInventorySnapshot(const QList<UsbHostDeviceInfo>& devices,
                                             const QString& trigger = {});

    static void installQtMessageHandler();
};

} // namespace FlashSpartan
