#include "Platform.h"

#include <QtGlobal>

namespace FlashSentry {

PlatformCapabilities Platform::capabilities()
{
    PlatformCapabilities caps;
#if defined(Q_OS_WIN)
    caps.platformName = QStringLiteral("Windows");
    caps.programmaticMount = true;
    caps.rawPartitionHash = true;
    caps.privilegedReadHelper = true;
    caps.udevMonitoring = false;
    caps.badUsbMonitoring = true;
    caps.usbmonCapture = true;
#elif defined(Q_OS_LINUX)
    caps.platformName = QStringLiteral("Linux");
    caps.programmaticMount = true;
    caps.rawPartitionHash = true;
    caps.privilegedReadHelper = true;
    caps.udevMonitoring = true;
    caps.badUsbMonitoring = true;
    caps.usbmonCapture = true;
#else
    caps.platformName = QStringLiteral("Unsupported Unix");
#endif
    return caps;
}

bool Platform::isWindows()
{
#if defined(Q_OS_WIN)
    return true;
#else
    return false;
#endif
}

bool Platform::isLinux()
{
#if defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

} // namespace FlashSentry
