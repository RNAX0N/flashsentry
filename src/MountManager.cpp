#include "MountManager.h"
#include "MountDBusUtil.h"
#include "MountOptionsUtil.h"

#ifdef Q_OS_WIN

#include "WinStorage.h"

#include <QMutexLocker>
#include <QDir>
#include <QStorageInfo>
#include <QTimer>
#include <qt_windows.h>

namespace FlashSpartan {

MountManager::MountManager(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<MountResult>("MountResult");
    qRegisterMetaType<UnmountResult>("UnmountResult");
    refreshMountStatus();
}

MountManager::~MountManager() = default;

bool MountManager::isAvailable() const
{
    return true;
}

QString MountManager::udisksVersion() const
{
    return QStringLiteral("Windows removable-volume APIs");
}

void MountManager::mount(const QString& deviceNode)
{
    mount(deviceNode, MountOptions{});
}

void MountManager::mount(const QString& deviceNode, const MountOptions& /*options*/)
{
    refreshMountStatus();

    MountResult result;
    result.deviceNode = deviceNode;
    result.mountPoint = getMountPoint(deviceNode);
    if (result.mountPoint.isEmpty()) {
        const QString normalized = WinStorage::normalizeVolumeRoot(deviceNode);
        if (WinStorage::isUsbFlashVolumeRoot(normalized)) {
            result.mountPoint = normalized;
        }
    }
    result.success = !result.mountPoint.isEmpty();
    if (!result.success) {
        result.errorMessage = QStringLiteral(
            "Volume is not mounted. Reconnect the drive or assign a letter in Disk Management.");
    } else {
        QMutexLocker locker(&m_mutex);
        m_mountPoints.insert(deviceNode, result.mountPoint);
        if (result.mountPoint != deviceNode) {
            m_mountPoints.insert(result.mountPoint, result.mountPoint);
        }
    }
    QTimer::singleShot(0, this, [this, result]() { emit mountCompleted(result); });
}

void MountManager::unmount(const QString& deviceNode)
{
    unmount(deviceNode, UnmountOptions{});
}

void MountManager::unmount(const QString& deviceNode, const UnmountOptions& options)
{
    UnmountResult result;
    result.deviceNode = deviceNode;
    result.forcedUnmount = options.force;

    QString error;
    const QString volumeRoot =
        WinStorage::normalizeVolumeRoot(getMountPoint(deviceNode).isEmpty() ? deviceNode
                                                                           : getMountPoint(deviceNode));
    if (WinStorage::ejectVolumeRoot(volumeRoot, options.force, &error)) {
        result.success = true;
        {
            QMutexLocker locker(&m_mutex);
            m_mountPoints.remove(deviceNode);
            if (volumeRoot != deviceNode) {
                m_mountPoints.remove(volumeRoot);
            }
        }
        emit mountStatusChanged(deviceNode, false, QString());
    } else {
        result.errorMessage = error.isEmpty()
            ? QStringLiteral("Failed to eject removable volume")
            : error;
    }

    QTimer::singleShot(0, this, [this, result]() { emit unmountCompleted(result); });
}

void MountManager::powerOff(const QString& deviceNode)
{
    QString error;
    const QString volumeRoot =
        WinStorage::normalizeVolumeRoot(getMountPoint(deviceNode).isEmpty() ? deviceNode
                                                                           : getMountPoint(deviceNode));
    const bool ok = WinStorage::ejectVolumeRoot(volumeRoot, false, &error);
    QTimer::singleShot(0, this, [this, deviceNode, ok, error]() {
        emit powerOffCompleted(deviceNode, ok,
                               ok ? QString() : (error.isEmpty()
                                                     ? QStringLiteral("Failed to eject device")
                                                     : error));
    });
}

QString MountManager::getMountPoint(const QString& deviceNode) const
{
    QMutexLocker locker(&m_mutex);
    return m_mountPoints.value(deviceNode);
}

bool MountManager::hasPendingOperations() const
{
    return false;
}

QStringList MountManager::mountedDevices() const
{
    QMutexLocker locker(&m_mutex);
    return m_mountPoints.keys();
}

void MountManager::refreshMountStatus()
{
    QHash<QString, QString> newMountPoints;
    for (const QStorageInfo& storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()
            || !WinStorage::isUsbFlashVolumeRoot(storage.rootPath())) {
            continue;
        }
        const QString root = QDir::toNativeSeparators(storage.rootPath());
        newMountPoints.insert(root, root);
    }

    QMutexLocker locker(&m_mutex);
    for (auto it = m_mountPoints.constBegin(); it != m_mountPoints.constEnd(); ++it) {
        if (!newMountPoints.contains(it.key())) {
            const QString device = it.key();
            locker.unlock();
            emit mountStatusChanged(device, false, QString());
            locker.relock();
        }
    }
    for (auto it = newMountPoints.constBegin(); it != newMountPoints.constEnd(); ++it) {
        if (!m_mountPoints.contains(it.key())) {
            const QString device = it.key();
            const QString mountPoint = it.value();
            locker.unlock();
            emit mountStatusChanged(device, true, mountPoint);
            locker.relock();
        }
    }
    m_mountPoints = newMountPoints;
}

QString MountManager::getFilesystemType(const QString& deviceNode) const
{
    const QStorageInfo storage(deviceNode);
    return QString::fromUtf8(storage.fileSystemType());
}

bool MountManager::isLoopDevice(const QString& /*deviceNode*/) const
{
    return false;
}

} // namespace FlashSpartan

#else

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusReply>
#include <QDBusPendingReply>
#include <QDBusMetaType>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>

#include "MountDBusUtil.h"
#include "MountOptionsUtil.h"

namespace FlashSpartan {

MountManager::MountManager(QObject* parent)
    : QObject(parent)
{
    // Register meta types for signals
    qRegisterMetaType<MountResult>("MountResult");
    qRegisterMetaType<UnmountResult>("UnmountResult");
    
    // Create UDisks2 manager interface
    m_udisksInterface = std::make_unique<QDBusInterface>(
        UDISKS2_SERVICE,
        UDISKS2_PATH,
        UDISKS2_MANAGER_IFACE,
        QDBusConnection::systemBus()
    );
    
    if (!m_udisksInterface->isValid()) {
        qWarning() << "MountManager: Failed to connect to UDisks2:"
                   << m_udisksInterface->lastError().message();
    }
    
    // Initial mount status refresh
    refreshMountStatus();
}

MountManager::~MountManager()
{
    // Cancel any pending operations
    QMutexLocker locker(&m_mutex);
    
    for (auto* watcher : m_pendingMounts.keys()) {
        delete watcher;
    }
    for (auto* watcher : m_pendingUnmounts.keys()) {
        delete watcher;
    }
    for (auto* watcher : m_pendingPowerOffs.keys()) {
        delete watcher;
    }
}

bool MountManager::isAvailable() const
{
    return m_udisksInterface && m_udisksInterface->isValid();
}

QString MountManager::udisksVersion() const
{
    if (!isAvailable()) {
        return QString();
    }
    
    QDBusInterface propsInterface(
        UDISKS2_SERVICE,
        "/org/freedesktop/UDisks2/Manager",
        DBUS_PROPERTIES_IFACE,
        QDBusConnection::systemBus()
    );
    
    QDBusReply<QVariant> reply = propsInterface.call(
        "Get", UDISKS2_MANAGER_IFACE, "Version"
    );
    
    if (reply.isValid()) {
        return reply.value().toString();
    }
    
    return QString();
}

void MountManager::mount(const QString& deviceNode)
{
    mount(deviceNode, MountOptions{});
}

void MountManager::mount(const QString& deviceNode, const MountOptions& options)
{
    auto fsInterface = createFilesystemInterface(deviceNode);
    
    if (!fsInterface || !fsInterface->isValid()) {
        MountResult result;
        result.deviceNode = deviceNode;
        result.success = false;
        result.errorMessage = "Device does not support filesystem operations";
        emit mountCompleted(result);
        return;
    }
    
    QVariantMap mountOptions = MountOptionsUtil::toUdisksMountOptions(options);
    
    QDBusPendingCall pendingCall = fsInterface->asyncCall("Mount", mountOptions);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(pendingCall, this);
    
    {
        QMutexLocker locker(&m_mutex);
        m_pendingMounts.insert(watcher, deviceNode);
    }
    
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &MountManager::onMountFinished);
}

void MountManager::unmount(const QString& deviceNode)
{
    unmount(deviceNode, UnmountOptions{});
}

void MountManager::unmount(const QString& deviceNode, const UnmountOptions& options)
{
    auto fsInterface = createFilesystemInterface(deviceNode);
    
    if (!fsInterface || !fsInterface->isValid()) {
        UnmountResult result;
        result.deviceNode = deviceNode;
        result.success = false;
        result.errorMessage = "Device does not support filesystem operations";
        emit unmountCompleted(result);
        return;
    }
    
    QVariantMap unmountOptions = MountOptionsUtil::toUdisksUnmountOptions(options);
    
    QDBusPendingCall pendingCall = fsInterface->asyncCall("Unmount", unmountOptions);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(pendingCall, this);
    
    {
        QMutexLocker locker(&m_mutex);
        m_pendingUnmounts.insert(watcher, deviceNode);
    }
    
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &MountManager::onUnmountFinished);
}

void MountManager::powerOff(const QString& deviceNode)
{
    QString blockPath = getBlockObjectPath(deviceNode);
    QString drivePath = getDriveObjectPath(blockPath);
    
    if (drivePath.isEmpty()) {
        emit powerOffCompleted(deviceNode, false, "Could not find drive for device");
        return;
    }
    
    auto driveInterface = createDriveInterface(drivePath);
    
    if (!driveInterface || !driveInterface->isValid()) {
        emit powerOffCompleted(deviceNode, false, "Could not access drive interface");
        return;
    }
    
    QVariantMap options;
    
    QDBusPendingCall pendingCall = driveInterface->asyncCall("PowerOff", options);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(pendingCall, this);
    
    {
        QMutexLocker locker(&m_mutex);
        m_pendingPowerOffs.insert(watcher, deviceNode);
    }
    
    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, &MountManager::onPowerOffFinished);
}

QString MountManager::getMountPoint(const QString& deviceNode) const
{
    QMutexLocker locker(&m_mutex);
    return m_mountPoints.value(deviceNode);
}

bool MountManager::hasPendingOperations() const
{
    QMutexLocker locker(&m_mutex);
    return !m_pendingMounts.isEmpty() || 
           !m_pendingUnmounts.isEmpty() ||
           !m_pendingPowerOffs.isEmpty();
}

QStringList MountManager::mountedDevices() const
{
    QMutexLocker locker(&m_mutex);
    QStringList devices;
    
    for (auto it = m_mountPoints.begin(); it != m_mountPoints.end(); ++it) {
        if (!it.value().isEmpty()) {
            devices.append(it.key());
        }
    }
    
    return devices;
}

void MountManager::refreshMountStatus()
{
    // Parse /proc/mounts to get current mount status
    QFile mounts("/proc/mounts");
    if (!mounts.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "MountManager: Failed to open /proc/mounts";
        return;
    }
    
    QHash<QString, QString> newMountPoints;
    
    while (!mounts.atEnd()) {
        QByteArray line = mounts.readLine();
        QList<QByteArray> parts = line.split(' ');
        
        if (parts.size() >= 2) {
            QString device = QString::fromUtf8(parts[0]);
            QString mountPoint = QString::fromUtf8(parts[1]);
            
            // Unescape special characters in mount point
            mountPoint.replace("\\040", " ");
            mountPoint.replace("\\011", "\t");
            mountPoint.replace("\\012", "\n");
            mountPoint.replace("\\134", "\\");
            
            if (device.startsWith("/dev/")) {
                newMountPoints.insert(device, mountPoint);
            }
        }
    }
    
    // Update mount points and emit changes
    {
        QMutexLocker locker(&m_mutex);
        
        // Check for unmounted devices
        for (auto it = m_mountPoints.begin(); it != m_mountPoints.end(); ++it) {
            if (!newMountPoints.contains(it.key())) {
                QString device = it.key();
                locker.unlock();
                emit mountStatusChanged(device, false, QString());
                locker.relock();
            }
        }
        
        // Check for newly mounted devices
        for (auto it = newMountPoints.begin(); it != newMountPoints.end(); ++it) {
            if (!m_mountPoints.contains(it.key()) || m_mountPoints[it.key()] != it.value()) {
                QString device = it.key();
                QString mp = it.value();
                locker.unlock();
                emit mountStatusChanged(device, true, mp);
                locker.relock();
            }
        }
        
        m_mountPoints = newMountPoints;
    }
}

QString MountManager::getFilesystemType(const QString& deviceNode) const
{
    QString objectPath = getBlockObjectPath(deviceNode);
    if (objectPath.isEmpty()) {
        return QString();
    }
    
    QDBusInterface propsInterface(
        UDISKS2_SERVICE,
        objectPath,
        DBUS_PROPERTIES_IFACE,
        QDBusConnection::systemBus()
    );
    
    QDBusReply<QVariant> reply = propsInterface.call(
        "Get", UDISKS2_BLOCK_IFACE, "IdType"
    );
    
    if (reply.isValid()) {
        return reply.value().toString();
    }
    
    return QString();
}

bool MountManager::isLoopDevice(const QString& deviceNode) const
{
    return deviceNode.startsWith("/dev/loop");
}

void MountManager::onMountFinished(QDBusPendingCallWatcher* watcher)
{
    QString deviceNode;
    
    {
        QMutexLocker locker(&m_mutex);
        deviceNode = m_pendingMounts.take(watcher);
    }
    
    MountResult result;
    result.deviceNode = deviceNode;
    
    QDBusPendingReply<QString> reply = *watcher;
    
    if (reply.isError()) {
        result.success = false;
        result.errorMessage = extractErrorMessage(reply.error());
        qWarning() << "MountManager: Mount failed for" << deviceNode
                   << "-" << result.errorMessage;
    } else {
        result.success = true;
        result.mountPoint = reply.value();
        
        {
            QMutexLocker locker(&m_mutex);
            m_mountPoints.insert(deviceNode, result.mountPoint);
        }
        
        qInfo() << "MountManager: Mounted" << deviceNode << "at" << result.mountPoint;
    }
    
    emit mountCompleted(result);
    watcher->deleteLater();
}

void MountManager::onUnmountFinished(QDBusPendingCallWatcher* watcher)
{
    QString deviceNode;
    
    {
        QMutexLocker locker(&m_mutex);
        deviceNode = m_pendingUnmounts.take(watcher);
    }
    
    UnmountResult result;
    result.deviceNode = deviceNode;
    
    QDBusPendingReply<> reply = *watcher;
    
    if (reply.isError()) {
        result.success = false;
        result.errorMessage = extractErrorMessage(reply.error());
        qWarning() << "MountManager: Unmount failed for" << deviceNode
                   << "-" << result.errorMessage;
    } else {
        result.success = true;
        
        {
            QMutexLocker locker(&m_mutex);
            m_mountPoints.remove(deviceNode);
        }
        
        qInfo() << "MountManager: Unmounted" << deviceNode;
    }
    
    emit unmountCompleted(result);
    watcher->deleteLater();
}

void MountManager::onPowerOffFinished(QDBusPendingCallWatcher* watcher)
{
    QString deviceNode;
    
    {
        QMutexLocker locker(&m_mutex);
        deviceNode = m_pendingPowerOffs.take(watcher);
    }
    
    QDBusPendingReply<> reply = *watcher;
    
    if (reply.isError()) {
        QString errorMsg = extractErrorMessage(reply.error());
        qWarning() << "MountManager: Power off failed for" << deviceNode
                   << "-" << errorMsg;
        emit powerOffCompleted(deviceNode, false, errorMsg);
    } else {
        qInfo() << "MountManager: Powered off" << deviceNode;
        emit powerOffCompleted(deviceNode, true, QString());
    }
    
    watcher->deleteLater();
}

QString MountManager::getBlockObjectPath(const QString& deviceNode) const
{
    // Convert device node to UDisks2 object path
    // /dev/sda1 -> /org/freedesktop/UDisks2/block_devices/sda1
    
    if (!deviceNode.startsWith("/dev/")) {
        return QString();
    }
    
    QString deviceName = deviceNode.mid(5);  // Remove "/dev/"
    deviceName.replace("/", "_");  // Handle things like /dev/disk/by-id/...
    
    return QString("/org/freedesktop/UDisks2/block_devices/%1").arg(deviceName);
}

QString MountManager::getDriveObjectPath(const QString& blockObjectPath) const
{
    if (blockObjectPath.isEmpty()) {
        return QString();
    }
    
    QDBusInterface propsInterface(
        UDISKS2_SERVICE,
        blockObjectPath,
        DBUS_PROPERTIES_IFACE,
        QDBusConnection::systemBus()
    );
    
    QDBusReply<QVariant> reply = propsInterface.call(
        "Get", UDISKS2_BLOCK_IFACE, "Drive"
    );
    
    if (reply.isValid()) {
        QDBusObjectPath drivePath = qvariant_cast<QDBusObjectPath>(reply.value());
        return drivePath.path();
    }
    
    return QString();
}

std::unique_ptr<QDBusInterface> MountManager::createBlockInterface(const QString& deviceNode) const
{
    QString objectPath = getBlockObjectPath(deviceNode);
    if (objectPath.isEmpty()) {
        return nullptr;
    }
    
    auto interface = std::make_unique<QDBusInterface>(
        UDISKS2_SERVICE,
        objectPath,
        UDISKS2_BLOCK_IFACE,
        QDBusConnection::systemBus()
    );
    
    if (!interface->isValid()) {
        return nullptr;
    }
    
    return interface;
}

std::unique_ptr<QDBusInterface> MountManager::createFilesystemInterface(const QString& deviceNode) const
{
    QString objectPath = getBlockObjectPath(deviceNode);
    if (objectPath.isEmpty()) {
        return nullptr;
    }
    
    auto interface = std::make_unique<QDBusInterface>(
        UDISKS2_SERVICE,
        objectPath,
        UDISKS2_FS_IFACE,
        QDBusConnection::systemBus()
    );
    
    if (!interface->isValid()) {
        return nullptr;
    }
    
    return interface;
}

std::unique_ptr<QDBusInterface> MountManager::createDriveInterface(const QString& driveObjectPath) const
{
    if (driveObjectPath.isEmpty() || driveObjectPath == "/") {
        return nullptr;
    }
    
    auto interface = std::make_unique<QDBusInterface>(
        UDISKS2_SERVICE,
        driveObjectPath,
        UDISKS2_DRIVE_IFACE,
        QDBusConnection::systemBus()
    );
    
    if (!interface->isValid()) {
        return nullptr;
    }
    
    return interface;
}

QString MountManager::extractErrorMessage(const QDBusError& error) const
{
    return MountDBusUtil::formatMountError(error.message());
}

} // namespace FlashSpartan

#endif