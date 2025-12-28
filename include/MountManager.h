#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QHash>
#include <QMutex>
#include <memory>

#include "Types.h"

namespace FlashSentry {

/**
 * @brief MountManager - Manages device mounting via UDisks2 D-Bus API
 * 
 * Uses the system D-Bus to communicate with UDisks2, which handles
 * privilege escalation via polkit. This avoids the need to run the
 * application as root.
 * 
 * All operations are asynchronous to prevent blocking the UI.
 */
class MountManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Mount operation result
     */
    struct MountResult {
        QString deviceNode;
        QString mountPoint;
        bool success = false;
        QString errorMessage;
    };

    /**
     * @brief Unmount operation result
     */
    struct UnmountResult {
        QString deviceNode;
        bool success = false;
        QString errorMessage;
        bool forcedUnmount = false;
    };

    /**
     * @brief Mount options
     */
    struct MountOptions {
        QString filesystem;          // Force specific filesystem type
        bool readOnly = false;       // Mount read-only
        bool noExec = true;          // Disable execution (security)
        bool noSuid = true;          // Disable setuid bits (security)
        bool sync = false;           // Synchronous I/O
        QStringList extraOptions;    // Additional mount options
    };

    /**
     * @brief Unmount options
     */
    struct UnmountOptions {
        bool force = false;          // Force unmount even if busy
        bool lazy = false;           // Lazy unmount (detach from filesystem)
    };

    explicit MountManager(QObject* parent = nullptr);
    ~MountManager() override;

    // Prevent copying
    MountManager(const MountManager&) = delete;
    MountManager& operator=(const MountManager&) = delete;

    /**
     * @brief Check if UDisks2 is available
     */
    bool isAvailable() const;

    /**
     * @brief Get UDisks2 version
     */
    QString udisksVersion() const;

    /**
     * @brief Mount a device asynchronously
     * @param deviceNode Device node path (e.g., /dev/sdb1)
     * @param options Mount options
     */
    void mount(const QString& deviceNode);
    void mount(const QString& deviceNode, const MountOptions& options);

    /**
     * @brief Unmount a device asynchronously
     * @param deviceNode Device node path
     * @param options Unmount options
     */
    void unmount(const QString& deviceNode);
    void unmount(const QString& deviceNode, const UnmountOptions& options);

    /**
     * @brief Power off a device (eject)
     * @param deviceNode Device node path
     */
    void powerOff(const QString& deviceNode);

    /**
     * @brief Check if a device is currently mounted
     * @param deviceNode Device node path
     * @return Mount point if mounted, empty string otherwise
     */
    QString getMountPoint(const QString& deviceNode) const;

    /**
     * @brief Check if any mount/unmount operations are pending
     */
    bool hasPendingOperations() const;

    /**
     * @brief Get currently mounted devices managed by this instance
     */
    QStringList mountedDevices() const;

    /**
     * @brief Refresh mount status from system
     */
    void refreshMountStatus();

    /**
     * @brief Get filesystem type for a device
     */
    QString getFilesystemType(const QString& deviceNode) const;

    /**
     * @brief Check if device is a loop device (e.g., mounted ISO)
     */
    bool isLoopDevice(const QString& deviceNode) const;

signals:
    /**
     * @brief Emitted when mount operation completes
     */
    void mountCompleted(const FlashSentry::MountManager::MountResult& result);

    /**
     * @brief Emitted when unmount operation completes
     */
    void unmountCompleted(const FlashSentry::MountManager::UnmountResult& result);

    /**
     * @brief Emitted when power off completes
     */
    void powerOffCompleted(const QString& deviceNode, bool success, const QString& error);

    /**
     * @brief Emitted when mount status changes
     */
    void mountStatusChanged(const QString& deviceNode, bool mounted, const QString& mountPoint);

    /**
     * @brief Emitted on errors
     */
    void error(const QString& deviceNode, const QString& message);

private slots:
    void onMountFinished(QDBusPendingCallWatcher* watcher);
    void onUnmountFinished(QDBusPendingCallWatcher* watcher);
    void onPowerOffFinished(QDBusPendingCallWatcher* watcher);

private:
    /**
     * @brief Get UDisks2 block device object path
     */
    QString getBlockObjectPath(const QString& deviceNode) const;

    /**
     * @brief Get UDisks2 drive object path for a block device
     */
    QString getDriveObjectPath(const QString& blockObjectPath) const;

    /**
     * @brief Create D-Bus interface for a block device
     */
    std::unique_ptr<QDBusInterface> createBlockInterface(const QString& deviceNode) const;

    /**
     * @brief Create D-Bus interface for filesystem operations
     */
    std::unique_ptr<QDBusInterface> createFilesystemInterface(const QString& deviceNode) const;

    /**
     * @brief Create D-Bus interface for drive operations
     */
    std::unique_ptr<QDBusInterface> createDriveInterface(const QString& driveObjectPath) const;

    /**
     * @brief Convert mount options to D-Bus variant map
     */
    QVariantMap mountOptionsToVariant(const MountOptions& options) const;

    /**
     * @brief Convert unmount options to D-Bus variant map
     */
    QVariantMap unmountOptionsToVariant(const UnmountOptions& options) const;

    /**
     * @brief Extract error message from D-Bus error
     */
    QString extractErrorMessage(const QDBusError& error) const;

    // D-Bus connection
    std::unique_ptr<QDBusInterface> m_udisksInterface;

    // Pending operation tracking
    mutable QMutex m_mutex;
    QHash<QDBusPendingCallWatcher*, QString> m_pendingMounts;
    QHash<QDBusPendingCallWatcher*, QString> m_pendingUnmounts;
    QHash<QDBusPendingCallWatcher*, QString> m_pendingPowerOffs;

    // Mount status tracking
    QHash<QString, QString> m_mountPoints;  // deviceNode -> mountPoint

    // UDisks2 D-Bus constants
    static constexpr const char* UDISKS2_SERVICE = "org.freedesktop.UDisks2";
    static constexpr const char* UDISKS2_PATH = "/org/freedesktop/UDisks2";
    static constexpr const char* UDISKS2_MANAGER_IFACE = "org.freedesktop.UDisks2.Manager";
    static constexpr const char* UDISKS2_BLOCK_IFACE = "org.freedesktop.UDisks2.Block";
    static constexpr const char* UDISKS2_FS_IFACE = "org.freedesktop.UDisks2.Filesystem";
    static constexpr const char* UDISKS2_DRIVE_IFACE = "org.freedesktop.UDisks2.Drive";
    static constexpr const char* DBUS_PROPERTIES_IFACE = "org.freedesktop.DBus.Properties";
};

} // namespace FlashSentry

// Register types for signals
Q_DECLARE_METATYPE(FlashSentry::MountManager::MountResult)
Q_DECLARE_METATYPE(FlashSentry::MountManager::UnmountResult)