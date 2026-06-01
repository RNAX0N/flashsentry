#pragma once

#include <QString>

namespace FlashSpartan {

/** Resolves USBPcapCMD.exe from PATH and standard install locations (no PATH required). */
class UsbPcapLocator {
public:
    static QString findUsbPcapCmdExecutable();
};

} // namespace FlashSpartan
