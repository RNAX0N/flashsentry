#pragma once

#include <QString>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace FlashSpartan::WinStorage {

#ifdef Q_OS_WIN

QString normalizeVolumeRoot(const QString& path);
QString volumeDevicePath(const QString& volumeRoot);
QString physicalDrivePathForVolume(const QString& volumeRoot);
QString physicalDrivePathForDeviceNode(const QString& deviceNode);

bool isPhysicalDrivePath(const QString& path);
bool isVolumePath(const QString& path);

HANDLE openDeviceHandle(const QString& devicePath, DWORD desiredAccess, DWORD* winErrorOut);
uint64_t deviceSizeBytes(HANDLE handle, DWORD* winErrorOut);
void closeDeviceHandle(HANDLE handle);

bool lockVolume(HANDLE volumeHandle, QString* error);
bool dismountVolumeHandle(HANDLE volumeHandle, QString* error);
bool ejectVolumeRoot(const QString& volumeRoot, bool force, QString* error);

/** Lock and dismount a lettered volume without ejecting media (for raw disk reads). */
bool dismountVolumeRootInPlace(const QString& volumeRoot, QString* error);

/** Removable lettered volume or USB mass-storage (including drives reported as "fixed"). */
bool isUsbFlashVolumeRoot(const QString& volumeRoot);

#endif

} // namespace FlashSpartan::WinStorage
