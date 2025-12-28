#include "DeviceMonitor.h"

#include <libudev.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <QDebug>
#include <QMutexLocker>
#include <QFile>
#include <QIODevice>

namespace FlashSentry {

DeviceMonitor::DeviceMonitor(QObject* parent)
    : QThread(parent)
{
    // Create a pipe for thread-safe wakeup
    if (pipe2(m_wakeupPipe, O_NONBLOCK | O_CLOEXEC) != 0) {
        qWarning() << "DeviceMonitor: Failed to create wakeup pipe";
        m_wakeupPipe[0] = m_wakeupPipe[1] = -1;
    }
}

DeviceMonitor::~DeviceMonitor()
{
    stopMonitoring();
    
    if (m_wakeupPipe[0] >= 0) close(m_wakeupPipe[0]);
    if (m_wakeupPipe[1] >= 0) close(m_wakeupPipe[1]);
}

void DeviceMonitor::startMonitoring()
{
    if (m_running.load()) {
        qWarning() << "DeviceMonitor: Already running";
        return;
    }
    
    m_running.store(true);
    start(QThread::NormalPriority);
}

void DeviceMonitor::stopMonitoring()
{
    if (!m_running.load()) return;
    
    m_running.store(false);
    
    // Wake up the poll() call
    if (m_wakeupPipe[1] >= 0) {
        char c = 'x';
        [[maybe_unused]] auto ret = write(m_wakeupPipe[1], &c, 1);
    }
    
    // Wait for thread to finish
    if (!wait(5000)) {
        qWarning() << "DeviceMonitor: Thread did not stop gracefully, terminating";
        terminate();
        wait();
    }
}

QList<DeviceInfo> DeviceMonitor::connectedDevices() const
{
    QMutexLocker locker(&m_devicesMutex);
    return m_devices.values();
}

std::optional<DeviceInfo> DeviceMonitor::getDevice(const QString& deviceNode) const
{
    QMutexLocker locker(&m_devicesMutex);
    auto it = m_devices.find(deviceNode);
    if (it != m_devices.end()) {
        return *it;
    }
    return std::nullopt;
}

void DeviceMonitor::rescan()
{
    m_rescanRequested.store(true);
    
    // Wake up the poll() call
    if (m_wakeupPipe[1] >= 0) {
        char c = 'r';
        [[maybe_unused]] auto ret = write(m_wakeupPipe[1], &c, 1);
    }
}

void DeviceMonitor::run()
{
    if (!initializeUdev()) {
        emit monitorError("Failed to initialize udev");
        return;
    }
    
    // Scan for existing devices first
    scanExistingDevices();
    
    int devCount;
    {
        QMutexLocker locker(&m_devicesMutex);
        devCount = m_devices.size();
    }
    emit initialScanComplete(devCount);
    
    // Get monitor file descriptor
    int udevFd = udev_monitor_get_fd(m_monitor);
    if (udevFd < 0) {
        emit monitorError("Failed to get udev monitor fd");
        cleanupUdev();
        return;
    }
    
    // Main event loop
    while (m_running.load()) {
        // Handle rescan requests
        if (m_rescanRequested.exchange(false)) {
            scanExistingDevices();
        }
        
        struct pollfd fds[2];
        fds[0].fd = udevFd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        
        fds[1].fd = m_wakeupPipe[0];
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        
        int nfds = (m_wakeupPipe[0] >= 0) ? 2 : 1;
        
        int ret = poll(fds, nfds, POLL_TIMEOUT_MS);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            emit monitorError(QString("poll() failed: %1").arg(strerror(errno)));
            break;
        }
        
        // Drain wakeup pipe if signaled
        if (nfds > 1 && (fds[1].revents & POLLIN)) {
            char buf[16];
            while (read(m_wakeupPipe[0], buf, sizeof(buf)) > 0) {}
        }
        
        // Process udev events
        if (fds[0].revents & POLLIN) {
            struct udev_device* dev = udev_monitor_receive_device(m_monitor);
            if (dev) {
                processUdevEvent(dev);
                udev_device_unref(dev);
            }
        }
    }
    
    cleanupUdev();
}

bool DeviceMonitor::initializeUdev()
{
    m_udev = udev_new();
    if (!m_udev) {
        qCritical() << "DeviceMonitor: Failed to create udev context";
        return false;
    }
    
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor) {
        qCritical() << "DeviceMonitor: Failed to create udev monitor";
        udev_unref(m_udev);
        m_udev = nullptr;
        return false;
    }
    
    // Filter for block device partitions
    if (udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "block", "partition") < 0) {
        qWarning() << "DeviceMonitor: Failed to add subsystem filter";
    }
    
    // Enable receiving
    if (udev_monitor_enable_receiving(m_monitor) < 0) {
        qCritical() << "DeviceMonitor: Failed to enable monitor receiving";
        cleanupUdev();
        return false;
    }
    
    return true;
}

void DeviceMonitor::cleanupUdev()
{
    if (m_monitor) {
        udev_monitor_unref(m_monitor);
        m_monitor = nullptr;
    }
    if (m_udev) {
        udev_unref(m_udev);
        m_udev = nullptr;
    }
}

void DeviceMonitor::scanExistingDevices()
{
    if (!m_udev) return;
    
    struct udev_enumerate* enumerate = udev_enumerate_new(m_udev);
    if (!enumerate) return;
    
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_add_match_property(enumerate, "DEVTYPE", "partition");
    udev_enumerate_scan_devices(enumerate);
    
    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry;
    
    QSet<QString> currentNodes;
    
    udev_list_entry_foreach(entry, devices) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* dev = udev_device_new_from_syspath(m_udev, path);
        
        if (dev && isUsbStoragePartition(dev)) {
            DeviceInfo info = extractDeviceInfo(dev);
            currentNodes.insert(info.deviceNode);
            
            QMutexLocker locker(&m_devicesMutex);
            if (!m_devices.contains(info.deviceNode)) {
                m_devices.insert(info.deviceNode, info);
                locker.unlock();
                emit deviceConnected(info);
            } else if (m_devices[info.deviceNode].isMounted != info.isMounted) {
                m_devices[info.deviceNode] = info;
                locker.unlock();
                emit deviceChanged(info);
            }
        }
        
        if (dev) udev_device_unref(dev);
    }
    
    // Check for removed devices
    {
        QMutexLocker locker(&m_devicesMutex);
        QStringList toRemove;
        for (const auto& node : m_devices.keys()) {
            if (!currentNodes.contains(node)) {
                toRemove.append(node);
            }
        }
        for (const auto& node : toRemove) {
            m_devices.remove(node);
            locker.unlock();
            emit deviceDisconnected(node);
            locker.relock();
        }
    }
    
    udev_enumerate_unref(enumerate);
}

void DeviceMonitor::processUdevEvent(struct udev_device* dev)
{
    if (!dev) return;
    
    QString action = udev_device_get_action(dev);
    QString devNode = udev_device_get_devnode(dev);
    
    if (devNode.isEmpty()) return;
    
    if (!isUsbStoragePartition(dev)) return;
    
    if (action == "add") {
        DeviceInfo info = extractDeviceInfo(dev);
        
        {
            QMutexLocker locker(&m_devicesMutex);
            m_devices.insert(info.deviceNode, info);
        }
        
        emit deviceConnected(info);
        
    } else if (action == "remove") {
        {
            QMutexLocker locker(&m_devicesMutex);
            m_devices.remove(devNode);
        }
        
        emit deviceDisconnected(devNode);
        
    } else if (action == "change") {
        DeviceInfo info = extractDeviceInfo(dev);
        
        {
            QMutexLocker locker(&m_devicesMutex);
            m_devices[info.deviceNode] = info;
        }
        
        emit deviceChanged(info);
    }
}

DeviceInfo DeviceMonitor::extractDeviceInfo(struct udev_device* dev)
{
    DeviceInfo info;
    
    info.deviceNode = udev_device_get_devnode(dev);
    
    // Get parent device node (the whole disk)
    struct udev_device* parent = udev_device_get_parent_with_subsystem_devtype(dev, "block", "disk");
    if (parent) {
        info.parentDevice = udev_device_get_devnode(parent);
    }
    
    // Get USB parent for vendor/model info
    struct udev_device* usb = getUsbParent(dev);
    if (usb) {
        info.vendor = getSysAttr(usb, "manufacturer");
        if (info.vendor.isEmpty()) {
            info.vendor = getProperty(dev, "ID_VENDOR");
        }
        
        info.model = getSysAttr(usb, "product");
        if (info.model.isEmpty()) {
            info.model = getProperty(dev, "ID_MODEL");
        }
        
        info.serial = getSysAttr(usb, "serial");
        if (info.serial.isEmpty()) {
            info.serial = getProperty(dev, "ID_SERIAL_SHORT");
        }
    }
    
    // Filesystem info
    info.fsType = getProperty(dev, "ID_FS_TYPE");
    info.label = getProperty(dev, "ID_FS_LABEL");
    
    // Size (from sysfs)
    QString sizeStr = getSysAttr(dev, "size");
    if (!sizeStr.isEmpty()) {
        // Size is in 512-byte sectors
        info.sizeBytes = sizeStr.toULongLong() * 512;
    }
    
    // Mount status - check /proc/mounts
    QFile mounts("/proc/mounts");
    if (mounts.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = mounts.readAll();
        QString devNodeSearch = info.deviceNode + " ";
        
        for (const auto& line : data.split('\n')) {
            if (line.startsWith(info.deviceNode.toUtf8() + " ")) {
                info.isMounted = true;
                QList<QByteArray> parts = line.split(' ');
                if (parts.size() >= 2) {
                    info.mountPoint = QString::fromUtf8(parts[1]);
                    // Unescape mount point (handles spaces, etc.)
                    info.mountPoint.replace("\\040", " ");
                    info.mountPoint.replace("\\011", "\t");
                }
                break;
            }
        }
    }
    
    info.isRemovable = (getProperty(dev, "ID_BUS") == "usb");
    
    return info;
}

bool DeviceMonitor::isUsbStoragePartition(struct udev_device* dev)
{
    // Must be a partition
    QString devType = udev_device_get_devtype(dev);
    if (devType != "partition") return false;
    
    // Check if it's a USB device
    QString bus = getProperty(dev, "ID_BUS");
    if (bus != "usb") return false;
    
    // Additional check: must have a USB parent
    struct udev_device* usb = getUsbParent(dev);
    if (!usb) return false;
    
    // Check it's a storage device (not a USB hub, etc.)
    QString usbDriver = getSysAttr(usb, "bDeviceClass");
    // Class 0 means defined at interface level, which is typical for storage
    // Class 8 is mass storage
    // We'll also accept if ID_USB_DRIVER is usb-storage
    QString driver = getProperty(dev, "ID_USB_DRIVER");
    
    return (driver == "usb-storage" || driver == "uas" || 
            usbDriver == "00" || usbDriver == "08" || usbDriver.isEmpty());
}

struct udev_device* DeviceMonitor::getUsbParent(struct udev_device* dev)
{
    return udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
}

QString DeviceMonitor::getProperty(struct udev_device* dev, const char* key)
{
    const char* value = udev_device_get_property_value(dev, key);
    return value ? QString::fromUtf8(value) : QString();
}

QString DeviceMonitor::getSysAttr(struct udev_device* dev, const char* key)
{
    const char* value = udev_device_get_sysattr_value(dev, key);
    return value ? QString::fromUtf8(value).trimmed() : QString();
}

} // namespace FlashSentry