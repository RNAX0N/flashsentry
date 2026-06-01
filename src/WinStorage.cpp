#include "WinStorage.h"

#ifdef Q_OS_WIN

#include <qt_windows.h>

#include <QDir>
#include <QRegularExpression>

#include <winioctl.h>
#include <cfgmgr32.h>

namespace FlashSpartan::WinStorage {

namespace {

QString driveLetterFromRoot(const QString& volumeRoot)
{
    QString root = QDir::toNativeSeparators(volumeRoot.trimmed());
    if (root.length() >= 2 && root.at(1) == QLatin1Char(':')) {
        return root.left(2).toUpper();
    }
    return {};
}

QString winErrorMessage(DWORD code)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                        | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len =
        FormatMessageW(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    QString msg;
    if (len > 0 && buffer) {
        msg = QString::fromWCharArray(buffer).trimmed();
        LocalFree(buffer);
    } else {
        msg = QStringLiteral("Win32 error %1").arg(code);
    }
    return msg;
}

} // namespace

QString normalizeVolumeRoot(const QString& path)
{
    const QString letter = driveLetterFromRoot(path);
    if (!letter.isEmpty()) {
        return letter + QLatin1Char('\\');
    }
    return QDir::toNativeSeparators(path.trimmed());
}

QString volumeDevicePath(const QString& volumeRoot)
{
    const QString letter = driveLetterFromRoot(volumeRoot);
    if (letter.isEmpty()) {
        return {};
    }
    return QStringLiteral("\\\\.\\") + letter;
}

QString physicalDrivePathForVolume(const QString& volumeRoot)
{
    const QString volPath = volumeDevicePath(volumeRoot);
    if (volPath.isEmpty()) {
        return {};
    }

    HANDLE hVol = CreateFileW(reinterpret_cast<LPCWSTR>(volPath.utf16()), 0,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                              nullptr);
    if (hVol == INVALID_HANDLE_VALUE) {
        return {};
    }

    STORAGE_DEVICE_NUMBER deviceNumber{};
    DWORD bytesReturned = 0;
    QString physicalPath;
    if (DeviceIoControl(hVol, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &deviceNumber,
                        sizeof(deviceNumber), &bytesReturned, nullptr)) {
        physicalPath =
            QStringLiteral("\\\\.\\PhysicalDrive%1").arg(deviceNumber.DeviceNumber);
    }
    CloseHandle(hVol);
    return physicalPath;
}

QString physicalDrivePathForDeviceNode(const QString& deviceNode)
{
    const QString trimmed = deviceNode.trimmed();
    if (isPhysicalDrivePath(trimmed)) {
        return trimmed;
    }
    if (isVolumePath(trimmed) || driveLetterFromRoot(trimmed).length() == 2) {
        return physicalDrivePathForVolume(trimmed);
    }
    return {};
}

bool isPhysicalDrivePath(const QString& path)
{
    static const QRegularExpression re(
        QString(R"(^\\\\\.\\PhysicalDrive\d+$)"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(QDir::toNativeSeparators(path.trimmed())).hasMatch();
}

bool isVolumePath(const QString& path)
{
    const QString native = QDir::toNativeSeparators(path.trimmed());
    static const QRegularExpression re(
        QString(R"(^\\\\\.\\[A-Za-z]:$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (re.match(native).hasMatch()) {
        return true;
    }
    return driveLetterFromRoot(native).length() == 2;
}

HANDLE openDeviceHandle(const QString& devicePath, DWORD desiredAccess, DWORD* winErrorOut)
{
    const QString native = QDir::toNativeSeparators(devicePath.trimmed());
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), desiredAccess,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE && winErrorOut) {
        *winErrorOut = GetLastError();
    }
    return handle;
}

uint64_t deviceSizeBytes(HANDLE handle, DWORD* winErrorOut)
{
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
        if (winErrorOut) {
            *winErrorOut = ERROR_INVALID_HANDLE;
        }
        return 0;
    }

    GET_LENGTH_INFORMATION lengthInfo{};
    DWORD bytesReturned = 0;
    if (DeviceIoControl(handle, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &lengthInfo,
                        sizeof(lengthInfo), &bytesReturned, nullptr)) {
        return static_cast<uint64_t>(lengthInfo.Length.QuadPart);
    }

    LARGE_INTEGER size{};
    if (GetFileSizeEx(handle, &size)) {
        return static_cast<uint64_t>(size.QuadPart);
    }

    if (winErrorOut) {
        *winErrorOut = GetLastError();
    }
    return 0;
}

void closeDeviceHandle(HANDLE handle)
{
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
        CloseHandle(handle);
    }
}

bool lockVolume(HANDLE volumeHandle, QString* error)
{
    DWORD bytesReturned = 0;
    if (DeviceIoControl(volumeHandle, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned,
                        nullptr)) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("Failed to lock volume: %1")
                     .arg(winErrorMessage(GetLastError()));
    }
    return false;
}

bool dismountVolumeHandle(HANDLE volumeHandle, QString* error)
{
    DWORD bytesReturned = 0;
    if (DeviceIoControl(volumeHandle, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0,
                        &bytesReturned, nullptr)) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("Failed to dismount volume: %1")
                     .arg(winErrorMessage(GetLastError()));
    }
    return false;
}

bool ejectVolumeRoot(const QString& volumeRoot, bool force, QString* error)
{
    Q_UNUSED(force)

    const QString volPath = volumeDevicePath(volumeRoot);
    if (volPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Invalid volume path");
        }
        return false;
    }

    HANDLE hVol = openDeviceHandle(volPath, GENERIC_READ | GENERIC_WRITE, nullptr);
    if (hVol == INVALID_HANDLE_VALUE) {
        hVol = openDeviceHandle(volPath, 0, nullptr);
    }
    if (hVol == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = QStringLiteral("Cannot open volume: %1").arg(winErrorMessage(GetLastError()));
        }
        return false;
    }

    lockVolume(hVol, nullptr);
    dismountVolumeHandle(hVol, nullptr);

    DWORD bytesReturned = 0;
    const bool ejected =
        DeviceIoControl(hVol, IOCTL_STORAGE_EJECT_MEDIA, nullptr, 0, nullptr, 0, &bytesReturned,
                        nullptr) != 0;
    CloseHandle(hVol);

    if (!ejected) {
        if (error) {
            *error = QStringLiteral("Eject failed: %1").arg(winErrorMessage(GetLastError()));
        }
        return false;
    }
    return true;
}

} // namespace FlashSpartan::WinStorage

#endif
