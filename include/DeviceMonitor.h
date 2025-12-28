#pragma once

#include <QThread>
#include <QMutex>
#include <QHash>
#include <QSet>
#include <memory>
#include <atomic>

#include "Types.h"

struct udev;
struct udev_monitor;
struct udev_device;

namespace FlashSentry {

/**
 * @brief DeviceMonitor - Monitors USB block devices via libudev
 * 
 * Runs in a dedicated thread and emits signals when USB storage devices
 * are connected or disconnected. Uses poll() for efficient event waiting
 * without busy-looping.
 * 
 * Thread-safe: All public methods can be called from any thread.
 */
class DeviceMonitor : public QThread {
    Q_OBJECT

public:
    explicit DeviceMonitor(QObject* parent = nullptr);
    ~DeviceMonitor() override;

    // Prevent copying
    DeviceMonitor(const DeviceMonitor&) = delete;
    DeviceMonitor& operator=(const DeviceMonitor&) = delete;

    /**
     * @brief Start monitoring for USB devices
     * Scans for existing devices before entering the event loop.
     */
    void startMonitoring();

    /**
     * @brief Stop monitoring gracefully
     * Thread-safe, can be called from any thread.
     */
    void stopMonitoring();

    /**
     * @brief Check if monitoring is currently active
     */
    bool isMonitoring() const { return m_running.load(); }

    /**
     * @brief Get currently tracked devices
     * Returns a snapshot of currently connected USB devices.
     */
    QList<DeviceInfo> connectedDevices() const;

    /**
     * @brief Get device info by device node
     * @param deviceNode e.g., "/dev/sdb1"
     * @return DeviceInfo if found, empty optional otherwise
     */
    std::optional<DeviceInfo> getDevice(const QString& deviceNode) const;

    /**
     * @brief Force a rescan of all connected USB devices
     */
    void rescan();

signals:
    /**
     * @brief Emitted when a USB partition is connected
     * Note: This is emitted for each partition, not each physical device.
     */
    void deviceConnected(const FlashSentry::DeviceInfo& device);

    /**
     * @brief Emitted when a USB partition is disconnected
     */
    void deviceDisconnected(const QString& deviceNode);

    /**
     * @brief Emitted when device properties change (e.g., mount status)
     */
    void deviceChanged(const FlashSentry::DeviceInfo& device);

    /**
     * @brief Emitted when a monitoring error occurs
     */
    void monitorError(const QString& error);

    /**
     * @brief Emitted after initial device scan completes
     */
    void initialScanComplete(int deviceCount);

protected:
    void run() override;

private:
    /**
     * @brief Initialize udev context and monitor
     * @return true if successful
     */
    bool initializeUdev();

    /**
     * @brief Cleanup udev resources
     */
    void cleanupUdev();

    /**
     * @brief Scan for existing USB devices
     */
    void scanExistingDevices();

    /**
     * @brief Process a udev event
     */
    void processUdevEvent(struct udev_device* dev);

    /**
     * @brief Extract device information from udev_device
     */
    DeviceInfo extractDeviceInfo(struct udev_device* dev);

    /**
     * @brief Check if a udev_device is a USB storage partition
     */
    bool isUsbStoragePartition(struct udev_device* dev);

    /**
     * @brief Get the parent USB device
     */
    struct udev_device* getUsbParent(struct udev_device* dev);

    /**
     * @brief Get a udev property safely
     */
    QString getProperty(struct udev_device* dev, const char* key);

    /**
     * @brief Get a udev sysattr safely
     */
    QString getSysAttr(struct udev_device* dev, const char* key);

    // Udev handles
    struct udev* m_udev = nullptr;
    struct udev_monitor* m_monitor = nullptr;

    // Thread control
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_rescanRequested{false};
    int m_wakeupPipe[2] = {-1, -1};  // For thread-safe wakeup

    // Device tracking
    mutable QMutex m_devicesMutex;
    QHash<QString, DeviceInfo> m_devices;  // deviceNode -> DeviceInfo

    // Configuration
    static constexpr int POLL_TIMEOUT_MS = 500;
};

} // namespace FlashSentry