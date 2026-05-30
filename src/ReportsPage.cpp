#include "ReportsPage.h"
#include "StyleManager.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace FlashSentry {

namespace {

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QString verifyKindLabel(VerifyHistoryKind kind)
{
    switch (kind) {
        case VerifyHistoryKind::IsoScan:
            return QStringLiteral("ISO");
        case VerifyHistoryKind::Manifest:
            return QStringLiteral("Watch");
        case VerifyHistoryKind::Hash:
        default:
            return QStringLiteral("Hash");
    }
}

} // namespace

ReportsPage::ReportsPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ReportsPage::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Reports"));
    title->setFont(FSFont(Heading2));
    title->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    layout->addWidget(title);

    auto* intro = new QLabel(QStringLiteral(
        "Verification history and append-only audit logs. Use Refresh after external "
        "changes or open log files in your editor."));
    intro->setWordWrap(true);
    intro->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(intro);

    auto* actions = new QHBoxLayout;
    auto* refreshBtn = new QPushButton(QStringLiteral("Refresh"));
    connect(refreshBtn, &QPushButton::clicked, this, &ReportsPage::onRefreshClicked);
    auto* openAuditBtn = new QPushButton(QStringLiteral("Open verification audit"));
    connect(openAuditBtn, &QPushButton::clicked, this, &ReportsPage::onOpenAuditClicked);
    auto* openPolicyBtn = new QPushButton(QStringLiteral("Open policy audit"));
    connect(openPolicyBtn, &QPushButton::clicked, this, &ReportsPage::onOpenPolicyAuditClicked);
    actions->addWidget(refreshBtn);
    actions->addWidget(openAuditBtn);
    actions->addWidget(openPolicyBtn);
    actions->addStretch();
    layout->addLayout(actions);

    m_auditPathLabel = new QLabel;
    m_policyPathLabel = new QLabel;
    for (QLabel* lb : {m_auditPathLabel, m_policyPathLabel}) {
        lb->setWordWrap(true);
        lb->setStyleSheet(QString("color: %1; font-size: 9pt;")
                              .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
        layout->addWidget(lb);
    }

    m_tabs = new QTabWidget;

    auto* verifyTab = new QWidget;
    auto* verifyLay = new QVBoxLayout(verifyTab);
    auto* verifyToolbar = new QHBoxLayout;
    m_verifyStatusFilter = new QComboBox;
    m_verifyStatusFilter->addItem(QStringLiteral("All statuses"), QString());
    m_verifyStatusFilter->addItem(QStringLiteral("Pass"), QStringLiteral("pass"));
    m_verifyStatusFilter->addItem(QStringLiteral("Fail"), QStringLiteral("fail"));
    m_verifyStatusFilter->addItem(QStringLiteral("Mismatch"), QStringLiteral("mismatch"));
    m_verifyStatusFilter->addItem(QStringLiteral("Error"), QStringLiteral("error"));
    connect(m_verifyStatusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ReportsPage::onVerifyFilterChanged);
    m_verifySearch = new QLineEdit;
    m_verifySearch->setPlaceholderText(QStringLiteral("Search device or summary…"));
    m_verifySearch->setClearButtonEnabled(true);
    connect(m_verifySearch, &QLineEdit::textChanged, this, &ReportsPage::onVerifyFilterChanged);
    verifyToolbar->addWidget(new QLabel(QStringLiteral("Status:")));
    verifyToolbar->addWidget(m_verifyStatusFilter);
    verifyToolbar->addWidget(m_verifySearch, 1);
    verifyLay->addLayout(verifyToolbar);

    m_verifyTable = new QTableWidget(0, 6);
    m_verifyTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Device"),
        QStringLiteral("Type"),
        QStringLiteral("Status"),
        QStringLiteral("Summary"),
        QStringLiteral("Duration"),
    });
    m_verifyTable->verticalHeader()->setVisible(false);
    m_verifyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_verifyTable->setAlternatingRowColors(true);
    m_verifyTable->setStyleSheet(FSStyle.dataTableStyleSheet());
    verifyLay->addWidget(m_verifyTable);
    m_tabs->addTab(verifyTab, QStringLiteral("Verification"));

    m_auditTable = new QTableWidget(0, 4);
    m_auditTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Category"),
        QStringLiteral("Event"),
        QStringLiteral("Detail"),
    });
    m_auditTable->verticalHeader()->setVisible(false);
    m_auditTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_auditTable->setAlternatingRowColors(true);
    m_auditTable->setStyleSheet(FSStyle.dataTableStyleSheet());
    m_tabs->addTab(m_auditTable, QStringLiteral("Verification audit"));

    m_policyTable = new QTableWidget(0, 5);
    m_policyTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Actor"),
        QStringLiteral("Action"),
        QStringLiteral("Target"),
        QStringLiteral("Detail"),
    });
    m_policyTable->verticalHeader()->setVisible(false);
    m_policyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_policyTable->setAlternatingRowColors(true);
    m_policyTable->setStyleSheet(FSStyle.dataTableStyleSheet());
    m_tabs->addTab(m_policyTable, QStringLiteral("Policy audit"));

    layout->addWidget(m_tabs, 1);

    scroll->setWidget(content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

void ReportsPage::setLogPaths(const QString& auditPath, const QString& policyAuditPath)
{
    m_auditPathLabel->setText(QStringLiteral("Verification audit: %1").arg(auditPath));
    m_policyPathLabel->setText(QStringLiteral("Policy audit: %1").arg(policyAuditPath));
}

void ReportsPage::setVerificationRows(const QList<VerifyHistoryEntry>& entries)
{
    m_verifyRows = entries;
    const QString statusFilter = m_verifyStatusFilter->currentData().toString();
    const QString needle = m_verifySearch->text().trimmed().toLower();

    QList<VerifyHistoryEntry> visible;
    for (const VerifyHistoryEntry& e : m_verifyRows) {
        if (!statusFilter.isEmpty() && e.status.compare(statusFilter, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!needle.isEmpty()) {
            const QString hay = QStringLiteral("%1 %2 %3 %4")
                                    .arg(e.deviceNode, e.deviceLabel, e.summary, e.detail)
                                    .toLower();
            if (!hay.contains(needle)) {
                continue;
            }
        }
        visible.append(e);
    }

    m_verifyTable->setRowCount(visible.size());
    for (int row = 0; row < visible.size(); ++row) {
        const VerifyHistoryEntry& e = visible.at(row);
        m_verifyTable->setItem(
            row, 0, readOnlyItem(e.timestamp.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
        const QString device = e.deviceLabel.isEmpty() ? e.deviceNode : e.deviceLabel;
        m_verifyTable->setItem(row, 1, readOnlyItem(device));
        m_verifyTable->setItem(row, 2, readOnlyItem(verifyKindLabel(e.kind)));
        m_verifyTable->setItem(row, 3, readOnlyItem(e.status));
        m_verifyTable->setItem(row, 4, readOnlyItem(e.summary));
        QString dur;
        if (e.durationMs > 0) {
            dur = QStringLiteral("%1 s").arg(e.durationMs / 1000.0, 0, 'f', 1);
        }
        m_verifyTable->setItem(row, 5, readOnlyItem(dur));
    }
    applyVerifyTableLayout();
}

void ReportsPage::setAuditRows(const QList<AuditLogRow>& rows)
{
    m_auditTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const AuditLogRow& r = rows.at(row);
        m_auditTable->setItem(
            row, 0, readOnlyItem(r.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
        m_auditTable->setItem(row, 1, readOnlyItem(r.category));
        m_auditTable->setItem(row, 2, readOnlyItem(r.event));
        m_auditTable->setItem(row, 3, readOnlyItem(r.detail));
    }
    applyAuditTableLayout(m_auditTable);
}

void ReportsPage::setPolicyAuditRows(const QList<AuditLogRow>& rows)
{
    m_policyTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const AuditLogRow& r = rows.at(row);
        m_policyTable->setItem(
            row, 0, readOnlyItem(r.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
        m_policyTable->setItem(row, 1, readOnlyItem(r.category));
        m_policyTable->setItem(row, 2, readOnlyItem(r.event));
        m_policyTable->setItem(row, 3, readOnlyItem(r.detail));
        m_policyTable->setItem(row, 4, readOnlyItem(r.source.isEmpty() ? QStringLiteral("—") : r.source));
    }
    applyAuditTableLayout(m_policyTable);
}

void ReportsPage::applyVerifyTableLayout()
{
    m_verifyTable->resizeColumnsToContents();
    if (m_verifyTable->columnCount() >= 5) {
        m_verifyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    }
}

void ReportsPage::applyAuditTableLayout(QTableWidget* table)
{
    table->resizeColumnsToContents();
    if (table->columnCount() >= 4) {
        table->horizontalHeader()->setSectionResizeMode(table->columnCount() - 1,
                                                        QHeaderView::Stretch);
    }
}

void ReportsPage::onRefreshClicked()
{
    emit refreshRequested();
}

void ReportsPage::onOpenAuditClicked()
{
    emit openAuditLogRequested();
}

void ReportsPage::onOpenPolicyAuditClicked()
{
    emit openPolicyAuditRequested();
}

void ReportsPage::onVerifyFilterChanged()
{
    setVerificationRows(m_verifyRows);
}

} // namespace FlashSentry
