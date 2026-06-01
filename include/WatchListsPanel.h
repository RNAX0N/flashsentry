#pragma once

#include <QWidget>

class QListWidget;
class QLabel;
class QPushButton;

namespace FlashSpartan {

struct DeviceInfo;

/**
 * @brief Overview of watch-list baselines across connected USB devices (README tab).
 */
class WatchListsPanel : public QWidget {
    Q_OBJECT

public:
    explicit WatchListsPanel(QWidget* parent = nullptr);

    void refresh(const QList<DeviceInfo>& devices,
                 const QHash<QString, int>& groupCountByDeviceNode);

signals:
    void editDeviceRequested(const QString& deviceNode);

private slots:
    void onEditClicked();
    void onSelectionChanged();

private:
    QListWidget* m_deviceList = nullptr;
    QLabel* m_hintLabel = nullptr;
    QPushButton* m_editBtn = nullptr;
};

} // namespace FlashSpartan
