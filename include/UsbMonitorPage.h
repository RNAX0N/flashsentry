#pragma once

#include "UiEventTypes.h"

#include <QWidget>

class QLabel;
class QTableWidget;
class QVBoxLayout;

namespace FlashSentry {

class UsbMonitorPage : public QWidget {
    Q_OBJECT

public:
    explicit UsbMonitorPage(QWidget* parent = nullptr);

    void setStats(const UsbMonitorStats& stats);
    void setDevices(const QList<UsbDeviceRow>& rows);
    void setEvents(const QList<UiEventEntry>& events);

signals:
    void deviceNameEdited(const QString& deviceNode, const QString& name);
    void deviceActionsRequested(const QString& deviceNode);
    void eventDetailsRequested(const UiEventEntry& entry);

private slots:
    void onDeviceCellChanged(int row, int column);
    void onDeviceActionsClicked();
    void onEventDetailsClicked();

private:
    void setupUi();
    void styleTables();
    QWidget* createStatCard(const QString& title, QLabel*& valueLabel);

    QLabel* m_connectedValue = nullptr;
    QLabel* m_allowedValue = nullptr;
    QLabel* m_blockedValue = nullptr;
    QLabel* m_eventsValue = nullptr;

    QTableWidget* m_deviceTable = nullptr;
    QTableWidget* m_eventsTable = nullptr;

    QList<UsbDeviceRow> m_deviceRows;
    QList<UiEventEntry> m_eventRows;
    bool m_blockDeviceTableSignals = false;
};

} // namespace FlashSentry
