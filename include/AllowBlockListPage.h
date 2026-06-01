#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;
class QLabel;

namespace FlashSpartan {

struct AllowBlockRow {
    QString uniqueId;
    QString driveKey;
    QString deviceNode;
    QString displayName;
    QString vendorModel;
    QString status;
    QString trustDetail;
    bool isConnected = false;
    bool isBlocked = false;
    bool isAllowed = false;
};

class AllowBlockListPage : public QWidget {
    Q_OBJECT

public:
    explicit AllowBlockListPage(QWidget* parent = nullptr);

    void setRows(const QList<AllowBlockRow>& rows);
    void setSummary(int allowed, int blocked, int total);

    QString currentFilterId() const;
    QString searchText() const;

signals:
    void allowRequested(const QString& uniqueId, const QString& driveKey);
    void blockRequested(const QString& uniqueId, const QString& driveKey, const QString& label);
    void unblockRequested(const QString& uniqueId, const QString& driveKey);
    void removeFromWhitelistRequested(const QString& uniqueId);
    void setTrustRequested(const QString& uniqueId, int trustLevel);
    void historyRequested(const QString& deviceNode);
    void filterChanged();

private slots:
    void onAllowClicked();
    void onBlockClicked();
    void onUnblockClicked();
    void onRemoveClicked();
    void onHistoryClicked();
    void onFilterChanged();

private:
    void setupUi();
    void applyTableLayout();
    int rowIndexForButton(QObject* sender) const;

    QLabel* m_allowedLabel = nullptr;
    QLabel* m_blockedLabel = nullptr;
    QLabel* m_totalLabel = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QList<AllowBlockRow> m_rows;
};

} // namespace FlashSpartan
