#include "UsbPcapLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace FlashSpartan {

namespace {

bool isExecutableFile(const QString& path)
{
    return QFileInfo::exists(path) && QFileInfo(path).isFile();
}

#ifdef Q_OS_WIN
QString programFilesUsbPcapCmd()
{
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"ProgramFiles", buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    const QString base = QString::fromWCharArray(buffer);
    const QString candidate = base + QStringLiteral("/USBPcap/USBPcapCMD.exe");
    return isExecutableFile(candidate) ? candidate : QString();
}

QString programFilesX86UsbPcapCmd()
{
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"ProgramFiles(x86)", buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    const QString base = QString::fromWCharArray(buffer);
    const QString candidate = base + QStringLiteral("/USBPcap/USBPcapCMD.exe");
    return isExecutableFile(candidate) ? candidate : QString();
}
#endif

} // namespace

QString UsbPcapLocator::findUsbPcapCmdExecutable()
{
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("USBPcapCMD.exe"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }

#ifdef Q_OS_WIN
    if (const QString pf = programFilesUsbPcapCmd(); !pf.isEmpty()) {
        return pf;
    }
    if (const QString pfx86 = programFilesX86UsbPcapCmd(); !pfx86.isEmpty()) {
        return pfx86;
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString override = env.value(QStringLiteral("FLASHSPARTAN_USBPCAP_CMD"));
    if (!override.isEmpty() && isExecutableFile(override)) {
        return QDir::toNativeSeparators(override);
    }
#endif

    return {};
}

} // namespace FlashSpartan
