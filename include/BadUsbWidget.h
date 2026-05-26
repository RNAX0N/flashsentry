#pragma once

#include "Types.h"

#include <QHash>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace FlashSentry {

class BadUsbWidget : public QWidget {
    Q_OBJECT

public:
    explicit BadUsbWidget(QWidget* parent = nullptr);

    void setMonitoringEnabled(bool enabled);
    void setBaselineCount(int count);
    void setDevices(const QList<HidDeviceInfo>& devices);
    void updateDevice(const HidDeviceInfo& device, bool trusted);
    void removeDevice(const QString& stableId);
    void addAnomaly(const BadUsbAnomalyResult& anomaly);
    void setCaptureStatus(const QString& message);

signals:
    void trustRequested(const QString& stableId);
    void captureRequested(const QString& stableId);
    void refreshRequested();
    void openCaptureFolderRequested();
    void logMessageRequested(const QString& message);

private:
    void setupUi();
    void refreshSummary();
    QString selectedStableId() const;
    int rowForStableId(const QString& stableId) const;

    QLabel* m_summaryLabel = nullptr;
    QLabel* m_captureLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QListWidget* m_anomalyList = nullptr;
    QPushButton* m_trustBtn = nullptr;
    QPushButton* m_captureBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_openCaptureBtn = nullptr;

    bool m_monitoringEnabled = false;
    int m_baselineCount = 0;
    QHash<QString, HidDeviceInfo> m_devices;
    QHash<QString, bool> m_trusted;
};

} // namespace FlashSentry
