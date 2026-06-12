#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <memory>

#ifndef Q_OS_WIN
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QVariantMap>
#endif

#include "MountTypes.h"
#include "Types.h"

namespace FlashSpartan {

/**
 * @brief MountManager - device mount status and mount operations.
 *
 * Linux: UDisks2 via system D-Bus. Windows: read-only status from QStorageInfo.
 */
class MountManager : public QObject {
    Q_OBJECT

public:
    using MountResult = FlashSpartan::MountResult;
    using UnmountResult = FlashSpartan::UnmountResult;
    using MountOptions = FlashSpartan::MountOptions;
    using UnmountOptions = FlashSpartan::UnmountOptions;

    explicit MountManager(QObject* parent = nullptr);
    ~MountManager() override;

    MountManager(const MountManager&) = delete;
    MountManager& operator=(const MountManager&) = delete;

    bool isAvailable() const;
    QString udisksVersion() const;

    void mount(const QString& deviceNode);
    void mount(const QString& deviceNode, const MountOptions& options);

    void unmount(const QString& deviceNode);
    void unmount(const QString& deviceNode, const UnmountOptions& options);

    void powerOff(const QString& deviceNode);

    QString getMountPoint(const QString& deviceNode) const;

    bool hasPendingOperations() const;

    QStringList mountedDevices() const;

    void refreshMountStatus();

    QString getFilesystemType(const QString& deviceNode) const;

    bool isLoopDevice(const QString& deviceNode) const;

signals:
    void mountCompleted(const FlashSpartan::MountManager::MountResult& result);
    void unmountCompleted(const FlashSpartan::MountManager::UnmountResult& result);
    void powerOffCompleted(const QString& deviceNode, bool success, const QString& error);
    void mountStatusChanged(const QString& deviceNode, bool mounted, const QString& mountPoint);
    void error(const QString& deviceNode, const QString& message);

#ifndef Q_OS_WIN
private slots:
    void onMountFinished(QDBusPendingCallWatcher* watcher);
    void onUnmountFinished(QDBusPendingCallWatcher* watcher);
    void onPowerOffFinished(QDBusPendingCallWatcher* watcher);

private:
    QString getBlockObjectPath(const QString& deviceNode) const;
    QString getDriveObjectPath(const QString& blockObjectPath) const;

    std::unique_ptr<QDBusInterface> createBlockInterface(const QString& deviceNode) const;
    std::unique_ptr<QDBusInterface> createFilesystemInterface(const QString& deviceNode) const;
    std::unique_ptr<QDBusInterface> createDriveInterface(const QString& driveObjectPath) const;

    QString extractErrorMessage(const QDBusError& error) const;

    std::unique_ptr<QDBusInterface> m_udisksInterface;

    QHash<QDBusPendingCallWatcher*, QString> m_pendingMounts;
    QHash<QDBusPendingCallWatcher*, QString> m_pendingUnmounts;
    QHash<QDBusPendingCallWatcher*, QString> m_pendingPowerOffs;

    static constexpr const char* UDISKS2_SERVICE = "org.freedesktop.UDisks2";
    static constexpr const char* UDISKS2_PATH = "/org/freedesktop/UDisks2";
    static constexpr const char* UDISKS2_MANAGER_IFACE = "org.freedesktop.UDisks2.Manager";
    static constexpr const char* UDISKS2_BLOCK_IFACE = "org.freedesktop.UDisks2.Block";
    static constexpr const char* UDISKS2_FS_IFACE = "org.freedesktop.UDisks2.Filesystem";
    static constexpr const char* UDISKS2_DRIVE_IFACE = "org.freedesktop.UDisks2.Drive";
    static constexpr const char* DBUS_PROPERTIES_IFACE = "org.freedesktop.DBus.Properties";
#else
private:
#endif
    mutable QMutex m_mutex;
    QHash<QString, QString> m_mountPoints;
};

} // namespace FlashSpartan
