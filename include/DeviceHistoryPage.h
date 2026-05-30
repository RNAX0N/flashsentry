#pragma once

#include "UiEventTypes.h"

#include <QWidget>

class QComboBox;
class QTableWidget;

namespace FlashSentry {

class DeviceHistoryPage : public QWidget {
    Q_OBJECT

public:
    explicit DeviceHistoryPage(QWidget* parent = nullptr);

    void setDeviceChoices(const QStringList& labels, const QStringList& deviceNodes);
    void setSelectedDevice(const QString& deviceNode);
    QString selectedDeviceNode() const;

    void setEvents(const QList<UiEventEntry>& events);

signals:
    void deviceSelectionChanged(const QString& deviceNode);
    void eventDetailsRequested(const UiEventEntry& entry);

private slots:
    void onDeviceComboChanged(int index);
    void onEventDetailsClicked();

private:
    void setupUi();
    void applyTableLayout();

    QComboBox* m_deviceCombo = nullptr;
    QTableWidget* m_eventsTable = nullptr;
    QList<UiEventEntry> m_eventRows;
};

} // namespace FlashSentry
