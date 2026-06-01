#pragma once

#include "Types.h"

#include <QHash>
#include <QMutex>
#include <QThread>
#include <atomic>
#include <optional>

struct udev;
struct udev_monitor;
struct udev_device;

namespace FlashSpartan {

class HidDeviceMonitor : public QThread {
    Q_OBJECT

public:
    explicit HidDeviceMonitor(QObject* parent = nullptr);
    ~HidDeviceMonitor() override;

    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_running.load(); }
    void rescan();

    QList<HidDeviceInfo> connectedDevices() const;
    std::optional<HidDeviceInfo> getDevice(const QString& stableId) const;

signals:
    void hidConnected(const FlashSpartan::HidDeviceInfo& device);
    void hidDisconnected(const QString& stableId);
    void hidChanged(const FlashSpartan::HidDeviceInfo& device);
    void monitorError(const QString& error);
    void initialScanComplete(int deviceCount);

protected:
    void run() override;

private:
    bool initializeUdev();
    void cleanupUdev();
    void scanExistingDevices();
    void processUdevEvent(struct udev_device* dev);
    bool isUsbHidInput(struct udev_device* dev) const;
    HidDeviceInfo extractDeviceInfo(struct udev_device* dev) const;
    struct udev_device* getUsbDeviceParent(struct udev_device* dev) const;
    struct udev_device* getUsbInterfaceParent(struct udev_device* dev) const;
    QString getProperty(struct udev_device* dev, const char* key) const;
    QString getSysAttr(struct udev_device* dev, const char* key) const;

    struct udev* m_udev = nullptr;
    struct udev_monitor* m_monitor = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_rescanRequested{false};
    int m_wakeupPipe[2] = {-1, -1};

    mutable QMutex m_devicesMutex;
    QHash<QString, HidDeviceInfo> m_devices;

    static constexpr int POLL_TIMEOUT_MS = 500;
};

} // namespace FlashSpartan
