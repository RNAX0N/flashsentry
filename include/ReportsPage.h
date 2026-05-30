#pragma once

#include "VerifyHistory.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QLabel;

namespace FlashSentry {

struct AuditLogRow {
    QDateTime time;
    QString category;
    QString event;
    QString detail;
    QString source;
};

class ReportsPage : public QWidget {
    Q_OBJECT

public:
    explicit ReportsPage(QWidget* parent = nullptr);

    void setVerificationRows(const QList<VerifyHistoryEntry>& entries);
    void setAuditRows(const QList<AuditLogRow>& rows);
    void setPolicyAuditRows(const QList<AuditLogRow>& rows);
    void setLogPaths(const QString& auditPath, const QString& policyAuditPath);

signals:
    void refreshRequested();
    void openAuditLogRequested();
    void openPolicyAuditRequested();

private slots:
    void onRefreshClicked();
    void onOpenAuditClicked();
    void onOpenPolicyAuditClicked();
    void onVerifyFilterChanged();

private:
    void setupUi();
    void applyVerifyTableLayout();
    void applyAuditTableLayout(QTableWidget* table);

    QLabel* m_auditPathLabel = nullptr;
    QLabel* m_policyPathLabel = nullptr;
    QTabWidget* m_tabs = nullptr;
    QLineEdit* m_verifySearch = nullptr;
    QComboBox* m_verifyStatusFilter = nullptr;
    QTableWidget* m_verifyTable = nullptr;
    QTableWidget* m_auditTable = nullptr;
    QTableWidget* m_policyTable = nullptr;
    QList<VerifyHistoryEntry> m_verifyRows;
};

} // namespace FlashSentry
