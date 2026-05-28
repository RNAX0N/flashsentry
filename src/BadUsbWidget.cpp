#include "BadUsbWidget.h"
#include "BadUsbAnalyzer.h"

#include <QDesktopServices>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>

namespace FlashSentry {

BadUsbWidget::BadUsbWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void BadUsbWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("BadUSB behavior monitor"));
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto* intro = new QLabel(QStringLiteral(
        "Baselines USB HID keyboards, mice, and other input interfaces. Alerts on new "
        "untrusted keyboards, keyboard+storage composites, interface drift, and rapid reconnects."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_summaryLabel = new QLabel;
    layout->addWidget(m_summaryLabel);

    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Device"),
                                        QStringLiteral("Capabilities"),
                                        QStringLiteral("VID:PID"),
                                        QStringLiteral("USB bus"),
                                        QStringLiteral("Baseline"),
                                        QStringLiteral("Node")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto* buttonRow = new QHBoxLayout;
    m_trustBtn = new QPushButton(QStringLiteral("Trust / add to baseline"));
    m_captureBtn = new QPushButton(QStringLiteral("Start usbmon capture"));
    m_refreshBtn = new QPushButton(QStringLiteral("Refresh HID devices"));
    m_openCaptureBtn = new QPushButton(QStringLiteral("Open capture folder"));
    buttonRow->addWidget(m_trustBtn);
    buttonRow->addWidget(m_captureBtn);
    buttonRow->addWidget(m_refreshBtn);
    buttonRow->addWidget(m_openCaptureBtn);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    m_captureLabel = new QLabel(QStringLiteral("Capture: idle"));
    layout->addWidget(m_captureLabel);

    auto* anomalyTitle = new QLabel(QStringLiteral("Anomaly log"));
    anomalyTitle->setFont(titleFont);
    layout->addWidget(anomalyTitle);

    m_anomalyList = new QListWidget;
    layout->addWidget(m_anomalyList, 1);

    connect(m_trustBtn, &QPushButton::clicked, this, [this]() {
        const QString id = selectedStableId();
        if (!id.isEmpty()) {
            emit trustRequested(id);
        }
    });
    connect(m_captureBtn, &QPushButton::clicked, this, [this]() {
        const QString id = selectedStableId();
        if (!id.isEmpty()) {
            emit captureRequested(id);
        }
    });
    connect(m_refreshBtn, &QPushButton::clicked, this, &BadUsbWidget::refreshRequested);
    connect(m_openCaptureBtn, &QPushButton::clicked, this, &BadUsbWidget::openCaptureFolderRequested);

    refreshSummary();
}

void BadUsbWidget::setMonitoringEnabled(bool enabled)
{
    m_monitoringEnabled = enabled;
    refreshSummary();
}

void BadUsbWidget::setBaselineCount(int count)
{
    m_baselineCount = count;
    refreshSummary();
}

void BadUsbWidget::setDevices(const QList<HidDeviceInfo>& devices)
{
    m_devices.clear();
    m_trusted.clear();
    m_table->setRowCount(0);
    for (const HidDeviceInfo& device : devices) {
        updateDevice(device, false);
    }
    refreshSummary();
}

void BadUsbWidget::updateDevice(const HidDeviceInfo& device, bool trusted)
{
    const QString stableId = device.stableId();
    m_devices.insert(stableId, device);
    m_trusted.insert(stableId, trusted);

    int row = rowForStableId(stableId);
    if (row < 0) {
        row = m_table->rowCount();
        m_table->insertRow(row);
    }

    auto setItem = [this, row](int column, const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setToolTip(text);
        m_table->setItem(row, column, item);
    };
    setItem(0, device.displayName());
    setItem(1, device.capabilities.join(QStringLiteral(", ")));
    setItem(2, QStringLiteral("%1:%2").arg(device.vendorId, device.productId));
    setItem(3, device.usbBus.isEmpty() ? QStringLiteral("-") : device.usbBus);
    setItem(4, trusted ? QStringLiteral("Trusted") : QStringLiteral("Untrusted"));
    setItem(5, device.devNode);
    m_table->item(row, 0)->setData(Qt::UserRole, stableId);
    refreshSummary();
}

void BadUsbWidget::removeDevice(const QString& stableId)
{
    m_devices.remove(stableId);
    m_trusted.remove(stableId);
    const int row = rowForStableId(stableId);
    if (row >= 0) {
        m_table->removeRow(row);
    }
    refreshSummary();
}

void BadUsbWidget::addAnomaly(const BadUsbAnomalyResult& anomaly)
{
    const QString line = QStringLiteral("[%1] %2: %3 (%4)")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")),
             BadUsbAnalyzer::severityLabel(anomaly.severity),
             anomaly.summary,
             anomaly.device.displayName());
    m_anomalyList->insertItem(0, line);
    if (!anomaly.detail.isEmpty()) {
        m_anomalyList->item(0)->setToolTip(anomaly.detail);
    }
}

void BadUsbWidget::setCaptureStatus(const QString& message)
{
    m_captureLabel->setText(QStringLiteral("Capture: %1").arg(message));
}

void BadUsbWidget::refreshSummary()
{
    m_summaryLabel->setText(QStringLiteral("%1 | Connected HID: %2 | Baselines: %3")
        .arg(m_monitoringEnabled ? QStringLiteral("Monitoring enabled")
                                 : QStringLiteral("Monitoring disabled"))
        .arg(m_devices.size())
        .arg(m_baselineCount));
}

QString BadUsbWidget::selectedStableId() const
{
    const auto items = m_table->selectedItems();
    if (items.isEmpty()) {
        return {};
    }
    const int row = items.first()->row();
    const QTableWidgetItem* first = m_table->item(row, 0);
    return first ? first->data(Qt::UserRole).toString() : QString();
}

int BadUsbWidget::rowForStableId(const QString& stableId) const
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QTableWidgetItem* item = m_table->item(row, 0);
        if (item && item->data(Qt::UserRole).toString() == stableId) {
            return row;
        }
    }
    return -1;
}

} // namespace FlashSentry
