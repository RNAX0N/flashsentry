#pragma once

#include "UiEventTypes.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;
class QLabel;

namespace FlashSpartan {

class AlertsPage : public QWidget {
    Q_OBJECT

public:
    explicit AlertsPage(QWidget* parent = nullptr);

    void setAlerts(const QList<UiEventEntry>& alerts);
    void setSummary(int total, int securityCount);

    QString filterKind() const;
    QString searchText() const;

signals:
    void filterChanged();
    void eventDetailsRequested(const UiEventEntry& entry);

private slots:
    void onFilterChanged();
    void onDetailsClicked();

private:
    void setupUi();
    void applyTableLayout();
    int rowIndexForButton(QObject* sender) const;

    QLabel* m_totalLabel = nullptr;
    QLabel* m_securityLabel = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QList<UiEventEntry> m_rows;
    QList<UiEventEntry> m_visibleRows;
};

} // namespace FlashSpartan
