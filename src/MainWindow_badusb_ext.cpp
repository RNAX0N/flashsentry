#include "MainWindow.h"
#include "AuditLog.h"
#include "BadUsbAnalyzer.h"
#include "Platform.h"

#include <QDateTime>
#include <QInputDialog>
#include <QMessageBox>

namespace FlashSentry {

void MainWindow::configureBadUsbMonitoring()
{
    const bool supported = Platform::capabilities().badUsbMonitoring;
    const bool enabled = m_settings.badUsbEnabled && supported;
    if (m_badUsbWidget) {
        m_badUsbWidget->setMonitoringEnabled(enabled);
        if (m_badUsbBaselineStore) {
            m_badUsbWidget->setBaselineCount(m_badUsbBaselineStore->allDevices().size());
        }
        if (m_settings.badUsbEnabled && !supported) {
            m_badUsbWidget->setCaptureStatus(
                QStringLiteral("BadUSB HID monitoring is not implemented on Windows yet"));
        }
    }
    if (!m_hidMonitor) {
        return;
    }
    if (enabled && !m_hidMonitor->isMonitoring()) {
        m_hidMonitor->startMonitoring();
    } else if (!enabled && m_hidMonitor->isMonitoring()) {
        m_hidMonitor->stopMonitoring();
    }
}

void MainWindow::onHidConnected(const HidDeviceInfo& device)
{
    processBadUsbDevice(device);
}

void MainWindow::onHidChanged(const HidDeviceInfo& device)
{
    processBadUsbDevice(device);
}

void MainWindow::onHidDisconnected(const QString& stableId)
{
    m_hidDashboardDevices.remove(stableId);
    refreshDeviceTable();
    if (m_badUsbWidget) {
        m_badUsbWidget->removeDevice(stableId);
    }
    logMessage(QStringLiteral("HID device disconnected: %1").arg(stableId), LogLevel::Info);
}

QStringList MainWindow::relatedStorageNodesForHid(const HidDeviceInfo& device) const
{
    QStringList nodes;
    if (!m_deviceMonitor) {
        return nodes;
    }
    for (const DeviceInfo& storage : m_deviceMonitor->connectedDevices()) {
        bool related = false;
        if (!device.serial.isEmpty() && !storage.serial.isEmpty()) {
            related = device.serial == storage.serial;
        }
        if (!related && !device.manufacturer.isEmpty() && !storage.vendor.isEmpty()) {
            related = storage.vendor.contains(device.manufacturer, Qt::CaseInsensitive)
                      || device.manufacturer.contains(storage.vendor, Qt::CaseInsensitive);
        }
        if (related) {
            nodes.append(storage.deviceNode);
        }
    }
    nodes.removeDuplicates();
    return nodes;
}

void MainWindow::processBadUsbDevice(const HidDeviceInfo& device)
{
    if (!m_settings.badUsbEnabled || !m_badUsbBaselineStore) {
        return;
    }

    const QString stableId = device.stableId();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QList<QDateTime>& history = m_hidConnectHistory[stableId];
    history.append(now);
    while (!history.isEmpty() && history.first().secsTo(now) > 10) {
        history.removeFirst();
    }

    auto baseline = m_badUsbBaselineStore->matchDevice(device);
    if (!baseline.has_value() && isVisible() && m_settings.badUsbConfirmAnomalies) {
        const HidDeviceCategory suggested = inferredHidDeviceCategory(device);
        const auto selected = promptForHidCategory(device, suggested);
        if (selected.has_value()) {
            m_badUsbBaselineStore->upsertBaseline(
                device, true, QStringLiteral("User-enrolled on first sight"), *selected);
            baseline = m_badUsbBaselineStore->matchDevice(device);
            logMessage(QStringLiteral("BadUSB enrolled %1 as %2")
                           .arg(device.displayName(), hidDeviceCategoryLabel(*selected)),
                       LogLevel::Security);
        }
    }
    const QStringList relatedStorage = relatedStorageNodesForHid(device);
    const BadUsbAnomalyResult anomaly = BadUsbAnalyzer::analyzeConnect(
        device, baseline, relatedStorage, history.size(), m_settings);

    const bool trusted = baseline.has_value() && baseline->trusted;
    m_hidDashboardDevices.insert(stableId, device);
    refreshDeviceTable();
    if (m_badUsbWidget) {
        m_badUsbWidget->updateDevice(device, trusted);
    }

    if (m_settings.badUsbAutoBaselineTrusted && !anomaly.anomalous && !baseline.has_value()) {
        m_badUsbBaselineStore->upsertBaseline(device, true, QStringLiteral("Auto-baselined"),
                                              inferredHidDeviceCategory(device));
    }

    if (!anomaly.anomalous) {
        logMessage(QStringLiteral("HID observed: %1 (%2)")
                       .arg(device.displayName(), device.capabilities.join(QStringLiteral(", "))),
                   LogLevel::Info);
        return;
    }

    logMessage(QStringLiteral("BadUSB anomaly [%1]: %2")
                   .arg(anomaly.ruleId, anomaly.summary),
               LogLevel::Security);
    AuditLog::appendBadUsbEvent(anomaly);
    if (m_badUsbWidget) {
        m_badUsbWidget->addAnomaly(anomaly);
    }
    if (m_trayIcon) {
        m_trayIcon->notifyBadUsbAnomaly(anomaly);
    }

    if (m_settings.badUsbUsbmonEnabled && m_settings.badUsbUsbmonOnAnomalyOnly && m_usbmonCapture) {
        m_usbmonCapture->startCapture(device, anomaly, m_settings.badUsbUsbmonCommand);
    }

    if (m_settings.badUsbConfirmAnomalies && isVisible()) {
        QMessageBox::warning(this, QStringLiteral("BadUSB behavior alert"),
                             QStringLiteral("%1\n\n%2\n\nDevice: %3")
                                 .arg(anomaly.summary, anomaly.detail, device.displayName()));
    }
}

void MainWindow::onBadUsbTrustRequested(const QString& stableId)
{
    if (!m_hidMonitor || !m_badUsbBaselineStore) {
        return;
    }
    const auto device = m_hidMonitor->getDevice(stableId);
    if (!device) {
        logMessage(QStringLiteral("Cannot baseline missing HID device: %1").arg(stableId),
                   LogLevel::Warning);
        return;
    }
    const auto selected = promptForHidCategory(*device, inferredHidDeviceCategory(*device));
    if (!selected.has_value()) {
        return;
    }
    if (m_badUsbBaselineStore->upsertBaseline(*device, true, QString(), *selected)) {
        logMessage(QStringLiteral("BadUSB baseline trusted: %1").arg(device->displayName()),
                   LogLevel::Security);
        if (m_badUsbWidget) {
            m_badUsbWidget->updateDevice(*device, true);
        }
    }
}

void MainWindow::onBadUsbCaptureRequested(const QString& stableId)
{
    if (!m_hidMonitor || !m_usbmonCapture) {
        return;
    }
    const auto device = m_hidMonitor->getDevice(stableId);
    if (!device) {
        return;
    }
    BadUsbAnomalyResult manual;
    manual.ruleId = QStringLiteral("manual");
    manual.summary = QStringLiteral("Manual BadUSB usbmon capture");
    manual.device = *device;
    manual.detectedAtUtc = QDateTime::currentDateTimeUtc();
    m_usbmonCapture->startCapture(*device, manual, m_settings.badUsbUsbmonCommand);
}

std::optional<HidDeviceCategory> MainWindow::promptForHidCategory(
    const HidDeviceInfo& device, HidDeviceCategory suggested) const
{
    QStringList labels;
    QList<HidDeviceCategory> categories = {
        HidDeviceCategory::Keyboard,
        HidDeviceCategory::Mouse,
        HidDeviceCategory::KeyboardMouseCombo,
        HidDeviceCategory::Touchpad,
        HidDeviceCategory::GameController,
        HidDeviceCategory::Receiver,
        HidDeviceCategory::OtherHid,
    };
    int current = 0;
    for (int i = 0; i < categories.size(); ++i) {
        labels.append(hidDeviceCategoryLabel(categories.at(i)));
        if (categories.at(i) == suggested) {
            current = i;
        }
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(
        const_cast<MainWindow*>(this),
        QStringLiteral("Enroll HID device"),
        QStringLiteral("Confirm what this device is:\n\n%1\nVID:PID %2:%3\nCapabilities: %4")
            .arg(device.displayName(),
                 device.vendorId,
                 device.productId,
                 device.capabilities.join(QStringLiteral(", "))),
        labels,
        current,
        false,
        &ok);
    if (!ok) {
        return std::nullopt;
    }
    const int index = labels.indexOf(selected);
    if (index < 0 || index >= categories.size()) {
        return std::nullopt;
    }
    return categories.at(index);
}

} // namespace FlashSentry
