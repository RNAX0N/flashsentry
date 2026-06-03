#include "UsbHostMonitor.h"

#ifdef Q_OS_WIN

#include "WinUsbEnumerator.h"

#include <QDebug>
#include <QMutexLocker>

namespace FlashSpartan {

UsbHostMonitor::UsbHostMonitor(QObject* parent)
    : QThread(parent)
{
}

UsbHostMonitor::~UsbHostMonitor()
{
    stopMonitoring();
}

void UsbHostMonitor::startMonitoring()
{
    if (m_running.load()) {
        return;
    }
    m_running.store(true);
    start(QThread::NormalPriority);
}

void UsbHostMonitor::stopMonitoring()
{
    if (!m_running.load()) {
        return;
    }
    m_running.store(false);
    if (!wait(5000)) {
        qWarning() << "UsbHostMonitor: thread did not stop within timeout";
    }
}

void UsbHostMonitor::rescan()
{
    m_rescanRequested.store(true);
}

QList<UsbHostDeviceInfo> UsbHostMonitor::connectedDevices() const
{
    QMutexLocker locker(&m_devicesMutex);
    return m_devices.values();
}

std::optional<UsbHostDeviceInfo> UsbHostMonitor::getDevice(const QString& instanceId) const
{
    QMutexLocker locker(&m_devicesMutex);
    const auto it = m_devices.constFind(instanceId);
    if (it == m_devices.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

void UsbHostMonitor::run()
{
    scanExistingDevices();
    {
        QMutexLocker locker(&m_devicesMutex);
        emit initialScanComplete(m_devices.size());
    }

    int idleTicks = 0;
    while (m_running.load()) {
        const bool rescanNow = m_rescanRequested.exchange(false);
        if (rescanNow) {
            scanExistingDevices();
            idleTicks = 0;
        } else {
            ++idleTicks;
            if (idleTicks * POLL_TIMEOUT_MS >= IDLE_SCAN_INTERVAL_MS) {
                scanExistingDevices();
                idleTicks = 0;
            }
        }
        msleep(POLL_TIMEOUT_MS);
    }
}

void UsbHostMonitor::scanExistingDevices()
{
    QHash<QString, UsbHostDeviceInfo> detected;
    for (const UsbHostDeviceInfo& info : WinUsbEnumerator::enumeratePresentUsbDevices()) {
        detected.insert(info.instanceId, info);
    }

    QList<UsbHostDeviceInfo> connected;
    QList<UsbHostDeviceInfo> changed;
    QStringList disconnected;
    {
        QMutexLocker locker(&m_devicesMutex);
        for (auto it = detected.constBegin(); it != detected.constEnd(); ++it) {
            if (!m_devices.contains(it.key())) {
                connected.append(it.value());
            } else if (m_devices.value(it.key()).category != it.value().category
                       || m_devices.value(it.key()).displayName != it.value().displayName) {
                changed.append(it.value());
            }
        }
        for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
            if (!detected.contains(it.key())) {
                disconnected.append(it.key());
            }
        }
        m_devices = detected;
    }

    for (const UsbHostDeviceInfo& info : connected) {
        emit usbHostConnected(info);
    }
    for (const UsbHostDeviceInfo& info : changed) {
        emit usbHostChanged(info);
    }
    for (const QString& id : disconnected) {
        emit usbHostDisconnected(id);
    }
}

} // namespace FlashSpartan

#else

namespace FlashSpartan {

UsbHostMonitor::UsbHostMonitor(QObject* parent)
    : QThread(parent)
{
}

UsbHostMonitor::~UsbHostMonitor() = default;

void UsbHostMonitor::startMonitoring() {}
void UsbHostMonitor::stopMonitoring() {}
void UsbHostMonitor::rescan() {}

QList<UsbHostDeviceInfo> UsbHostMonitor::connectedDevices() const
{
    return {};
}

std::optional<UsbHostDeviceInfo> UsbHostMonitor::getDevice(const QString&) const
{
    return std::nullopt;
}

void UsbHostMonitor::run() {}

} // namespace FlashSpartan

#endif
