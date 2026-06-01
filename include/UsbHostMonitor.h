#pragma once

#include "WinUsbEnumerator.h"

#include <optional>

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <atomic>

namespace FlashSpartan {

/**
 * @brief Enumerates USB devices attached to the host (Windows SetupAPI).
 *
 * Complements storage-volume monitoring and HID detail for security keys,
 * hubs, chargers, and other USB connections without a drive letter.
 */
class UsbHostMonitor : public QThread {
    Q_OBJECT

public:
    explicit UsbHostMonitor(QObject* parent = nullptr);
    ~UsbHostMonitor() override;

    void startMonitoring();
    void stopMonitoring();
    void rescan();

    bool isMonitoring() const { return m_running.load(); }

    QList<UsbHostDeviceInfo> connectedDevices() const;
    std::optional<UsbHostDeviceInfo> getDevice(const QString& instanceId) const;

signals:
    void usbHostConnected(const UsbHostDeviceInfo& device);
    void usbHostDisconnected(const QString& instanceId);
    void usbHostChanged(const UsbHostDeviceInfo& device);
    void initialScanComplete(int count);

protected:
    void run() override;

private:
    void scanExistingDevices();

    mutable QMutex m_devicesMutex;
    QHash<QString, UsbHostDeviceInfo> m_devices;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_rescanRequested{false};

    static constexpr int POLL_TIMEOUT_MS = 500;
};

} // namespace FlashSpartan
