#include "HidDeviceMonitor.h"

#ifdef Q_OS_WIN

#include <QMutexLocker>

namespace FlashSentry {

HidDeviceMonitor::HidDeviceMonitor(QObject* parent)
    : QThread(parent)
{
}

HidDeviceMonitor::~HidDeviceMonitor()
{
    stopMonitoring();
}

void HidDeviceMonitor::startMonitoring()
{
    if (m_running.load()) {
        return;
    }
    m_running.store(true);
    start(QThread::NormalPriority);
}

void HidDeviceMonitor::stopMonitoring()
{
    if (!m_running.load()) {
        return;
    }
    m_running.store(false);
    wait(3000);
}

void HidDeviceMonitor::rescan()
{
    emit initialScanComplete(0);
}

QList<HidDeviceInfo> HidDeviceMonitor::connectedDevices() const
{
    QMutexLocker locker(&m_devicesMutex);
    return m_devices.values();
}

std::optional<HidDeviceInfo> HidDeviceMonitor::getDevice(const QString& stableId) const
{
    QMutexLocker locker(&m_devicesMutex);
    const auto it = m_devices.constFind(stableId);
    if (it == m_devices.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

void HidDeviceMonitor::run()
{
    emit initialScanComplete(0);
    while (m_running.load()) {
        msleep(POLL_TIMEOUT_MS);
    }
}

bool HidDeviceMonitor::initializeUdev()
{
    return true;
}
void HidDeviceMonitor::cleanupUdev() {}
void HidDeviceMonitor::scanExistingDevices()
{
    emit initialScanComplete(0);
}
void HidDeviceMonitor::processUdevEvent(struct udev_device* /*dev*/) {}
bool HidDeviceMonitor::isUsbHidInput(struct udev_device* /*dev*/) const
{
    return false;
}
HidDeviceInfo HidDeviceMonitor::extractDeviceInfo(struct udev_device* /*dev*/) const
{
    return {};
}
struct udev_device* HidDeviceMonitor::getUsbDeviceParent(struct udev_device* /*dev*/) const
{
    return nullptr;
}
struct udev_device* HidDeviceMonitor::getUsbInterfaceParent(struct udev_device* /*dev*/) const
{
    return nullptr;
}
QString HidDeviceMonitor::getProperty(struct udev_device* /*dev*/, const char* /*key*/) const
{
    return {};
}
QString HidDeviceMonitor::getSysAttr(struct udev_device* /*dev*/, const char* /*key*/) const
{
    return {};
}

} // namespace FlashSentry

#else

#include <libudev.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <QDebug>
#include <QMutexLocker>
#include <QSet>

namespace FlashSentry {

HidDeviceMonitor::HidDeviceMonitor(QObject* parent)
    : QThread(parent)
{
    if (pipe2(m_wakeupPipe, O_NONBLOCK | O_CLOEXEC) != 0) {
        qWarning() << "HidDeviceMonitor: Failed to create wakeup pipe";
        m_wakeupPipe[0] = m_wakeupPipe[1] = -1;
    }
}

HidDeviceMonitor::~HidDeviceMonitor()
{
    stopMonitoring();
    if (m_wakeupPipe[0] >= 0) close(m_wakeupPipe[0]);
    if (m_wakeupPipe[1] >= 0) close(m_wakeupPipe[1]);
}

void HidDeviceMonitor::startMonitoring()
{
    if (m_running.load()) {
        return;
    }
    m_running.store(true);
    start(QThread::NormalPriority);
}

void HidDeviceMonitor::stopMonitoring()
{
    if (!m_running.load()) {
        return;
    }
    m_running.store(false);
    if (m_wakeupPipe[1] >= 0) {
        char c = 'x';
        [[maybe_unused]] const auto ret = write(m_wakeupPipe[1], &c, 1);
    }
    if (!wait(5000)) {
        qWarning() << "HidDeviceMonitor: Thread did not stop gracefully, terminating";
        terminate();
        wait();
    }
}

void HidDeviceMonitor::rescan()
{
    m_rescanRequested.store(true);
    if (m_wakeupPipe[1] >= 0) {
        char c = 'r';
        [[maybe_unused]] const auto ret = write(m_wakeupPipe[1], &c, 1);
    }
}

QList<HidDeviceInfo> HidDeviceMonitor::connectedDevices() const
{
    QMutexLocker locker(&m_devicesMutex);
    return m_devices.values();
}

std::optional<HidDeviceInfo> HidDeviceMonitor::getDevice(const QString& stableId) const
{
    QMutexLocker locker(&m_devicesMutex);
    const auto it = m_devices.constFind(stableId);
    if (it == m_devices.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

void HidDeviceMonitor::run()
{
    if (!initializeUdev()) {
        emit monitorError(QStringLiteral("Failed to initialize HID udev monitor"));
        m_running.store(false);
        return;
    }

    scanExistingDevices();
    {
        QMutexLocker locker(&m_devicesMutex);
        emit initialScanComplete(m_devices.size());
    }

    const int udevFd = udev_monitor_get_fd(m_monitor);
    if (udevFd < 0) {
        emit monitorError(QStringLiteral("Failed to get HID udev monitor fd"));
        cleanupUdev();
        m_running.store(false);
        return;
    }

    while (m_running.load()) {
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

        const int nfds = (m_wakeupPipe[0] >= 0) ? 2 : 1;
        const int ret = poll(fds, nfds, POLL_TIMEOUT_MS);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            emit monitorError(QStringLiteral("HID poll failed: %1").arg(strerror(errno)));
            break;
        }
        if (nfds > 1 && (fds[1].revents & POLLIN)) {
            char buf[16];
            while (read(m_wakeupPipe[0], buf, sizeof(buf)) > 0) {}
        }
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

bool HidDeviceMonitor::initializeUdev()
{
    m_udev = udev_new();
    if (!m_udev) {
        return false;
    }
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor) {
        udev_unref(m_udev);
        m_udev = nullptr;
        return false;
    }
    if (udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "input", nullptr) < 0) {
        qWarning() << "HidDeviceMonitor: Failed to add input subsystem filter";
    }
    if (udev_monitor_enable_receiving(m_monitor) < 0) {
        cleanupUdev();
        return false;
    }
    return true;
}

void HidDeviceMonitor::cleanupUdev()
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

void HidDeviceMonitor::scanExistingDevices()
{
    if (!m_udev) {
        return;
    }
    struct udev_enumerate* enumerate = udev_enumerate_new(m_udev);
    if (!enumerate) {
        return;
    }
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    QSet<QString> currentIds;
    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry = nullptr;
    udev_list_entry_foreach(entry, devices) {
        const char* path = udev_list_entry_get_name(entry);
        if (!path) {
            continue;
        }
        struct udev_device* dev = udev_device_new_from_syspath(m_udev, path);
        if (!dev) {
            continue;
        }
        if (isUsbHidInput(dev)) {
            const HidDeviceInfo info = extractDeviceInfo(dev);
            const QString stableId = info.stableId();
            currentIds.insert(stableId);
            bool changed = false;
            {
                QMutexLocker locker(&m_devicesMutex);
                changed = m_devices.contains(stableId);
                m_devices.insert(stableId, info);
            }
            if (changed) {
                emit hidChanged(info);
            } else {
                emit hidConnected(info);
            }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);

    QList<QString> removed;
    {
        QMutexLocker locker(&m_devicesMutex);
        for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
            if (!currentIds.contains(it.key())) {
                removed.append(it.key());
            }
        }
        for (const QString& stableId : removed) {
            m_devices.remove(stableId);
        }
    }
    for (const QString& stableId : removed) {
        emit hidDisconnected(stableId);
    }
}

void HidDeviceMonitor::processUdevEvent(struct udev_device* dev)
{
    if (!isUsbHidInput(dev)) {
        return;
    }
    const QString action = QString::fromUtf8(udev_device_get_action(dev) ?: "");
    const HidDeviceInfo info = extractDeviceInfo(dev);
    const QString stableId = info.stableId();

    if (action == QStringLiteral("remove")) {
        {
            QMutexLocker locker(&m_devicesMutex);
            m_devices.remove(stableId);
        }
        emit hidDisconnected(stableId);
        return;
    }

    bool existed = false;
    {
        QMutexLocker locker(&m_devicesMutex);
        existed = m_devices.contains(stableId);
        m_devices.insert(stableId, info);
    }
    if (existed) {
        emit hidChanged(info);
    } else {
        emit hidConnected(info);
    }
}

bool HidDeviceMonitor::isUsbHidInput(struct udev_device* dev) const
{
    if (!dev || !getUsbDeviceParent(dev)) {
        return false;
    }
    const QString devNode = QString::fromUtf8(udev_device_get_devnode(dev) ?: "");
    if (!devNode.startsWith(QStringLiteral("/dev/input/event"))) {
        return false;
    }
    const bool hasInputProp =
        getProperty(dev, "ID_INPUT_KEYBOARD") == QStringLiteral("1")
        || getProperty(dev, "ID_INPUT_MOUSE") == QStringLiteral("1")
        || getProperty(dev, "ID_INPUT_TOUCHPAD") == QStringLiteral("1")
        || getProperty(dev, "ID_INPUT_JOYSTICK") == QStringLiteral("1");
    struct udev_device* iface = getUsbInterfaceParent(dev);
    const QString cls = iface ? getSysAttr(iface, "bInterfaceClass") : QString();
    return hasInputProp || cls.compare(QStringLiteral("03"), Qt::CaseInsensitive) == 0;
}

HidDeviceInfo HidDeviceMonitor::extractDeviceInfo(struct udev_device* dev) const
{
    HidDeviceInfo info;
    info.sysPath = QString::fromUtf8(udev_device_get_syspath(dev) ?: "");
    info.devNode = QString::fromUtf8(udev_device_get_devnode(dev) ?: "");
    info.seenAtUtc = QDateTime::currentDateTimeUtc();

    struct udev_device* usb = getUsbDeviceParent(dev);
    struct udev_device* iface = getUsbInterfaceParent(dev);
    if (usb) {
        info.usbPath = QString::fromUtf8(udev_device_get_syspath(usb) ?: "");
        info.usbBus = getSysAttr(usb, "busnum");
        info.usbPort = QString::fromUtf8(udev_device_get_sysname(usb) ?: "");
        info.vendorId = getSysAttr(usb, "idVendor").toLower();
        info.productId = getSysAttr(usb, "idProduct").toLower();
        info.serial = getSysAttr(usb, "serial");
        info.manufacturer = getSysAttr(usb, "manufacturer");
        info.product = getSysAttr(usb, "product");
    }
    if (iface) {
        HidInterfaceInfo hidIface;
        hidIface.number = getSysAttr(iface, "bInterfaceNumber");
        hidIface.interfaceClass = getSysAttr(iface, "bInterfaceClass");
        hidIface.interfaceSubClass = getSysAttr(iface, "bInterfaceSubClass");
        hidIface.interfaceProtocol = getSysAttr(iface, "bInterfaceProtocol");
        hidIface.driver = QString::fromUtf8(udev_device_get_driver(iface) ?: "");
        info.driver = hidIface.driver;
        info.interfaces.append(hidIface);
    }

    if (getProperty(dev, "ID_INPUT_KEYBOARD") == QStringLiteral("1")) {
        info.capabilities.append(QStringLiteral("keyboard"));
    }
    if (getProperty(dev, "ID_INPUT_MOUSE") == QStringLiteral("1")) {
        info.capabilities.append(QStringLiteral("mouse"));
    }
    if (getProperty(dev, "ID_INPUT_TOUCHPAD") == QStringLiteral("1")) {
        info.capabilities.append(QStringLiteral("touchpad"));
    }
    if (getProperty(dev, "ID_INPUT_JOYSTICK") == QStringLiteral("1")) {
        info.capabilities.append(QStringLiteral("joystick"));
    }
    if (info.capabilities.isEmpty()) {
        info.capabilities.append(QStringLiteral("hid"));
    }
    info.capabilities.removeDuplicates();
    return info;
}

struct udev_device* HidDeviceMonitor::getUsbDeviceParent(struct udev_device* dev) const
{
    return udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
}

struct udev_device* HidDeviceMonitor::getUsbInterfaceParent(struct udev_device* dev) const
{
    return udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_interface");
}

QString HidDeviceMonitor::getProperty(struct udev_device* dev, const char* key) const
{
    const char* value = udev_device_get_property_value(dev, key);
    return value ? QString::fromUtf8(value) : QString();
}

QString HidDeviceMonitor::getSysAttr(struct udev_device* dev, const char* key) const
{
    const char* value = udev_device_get_sysattr_value(dev, key);
    return value ? QString::fromUtf8(value).trimmed() : QString();
}

} // namespace FlashSentry

#endif
