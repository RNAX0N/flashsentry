#pragma once

#include <QString>

namespace FlashSpartan {

struct PlatformCapabilities {
    bool udevMonitoring = false;
    bool rawPartitionHash = false;
    bool privilegedReadHelper = false;
    bool programmaticMount = false;
    bool badUsbMonitoring = false;
    bool usbmonCapture = false;

    QString platformName;
};

class Platform {
public:
    static PlatformCapabilities capabilities();
    static bool isWindows();
    static bool isLinux();
};

} // namespace FlashSpartan
