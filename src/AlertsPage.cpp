#include "AlertsPage.h"
#include "StyleManager.h"

#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace FlashSpartan {

namespace {

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QWidget* tableButtonCell(QPushButton* btn)
{
    btn->setFixedHeight(20);
    btn->setStyleSheet(FSStyle.compactTableButtonStyleSheet());
    auto* cell = new QWidget;
    auto* lay = new QHBoxLayout(cell);
    lay->setContentsMargins(2, 1, 2, 1);
    lay->addWidget(btn, 0, Qt::AlignCenter);
    return cell;
}

QString severityLabel(const QString& result)
{
    const QString r = result.toLower();
    if (r == QLatin1String("alert") || r == QLatin1String("security")) {
        return QStringLiteral("Security");
    }
    if (r == QLatin1String("warn") || r == QLatin1String("warning")) {
        return QStringLiteral("Warning");
    }
    if (r == QLatin1String("error") || r == QLatin1String("fail") || r == QLatin1String("failed")) {
        return QStringLiteral("Error");
    }
    if (r == QLatin1String("mismatch") || r == QLatin1String("fail")) {
        return QStringLiteral("Mismatch");
    }
    if (r == QLatin1String("blocked") || r == QLatin1String("rejected")) {
        return QStringLiteral("Blocked");
    }
    return QStringLiteral("Notice");
}

} // namespace

AlertsPage::AlertsPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AlertsPage::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Alerts"));
    title->setFont(FSFont(Heading2));
    title->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    layout->addWidget(title);

    auto* intro = new QLabel(QStringLiteral(
        "Security warnings, verification failures, blocks, and anomalies from this session "
        "and persisted verification history."));
    intro->setWordWrap(true);
    intro->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(intro);

    auto* stats = new QHBoxLayout;
    m_totalLabel = new QLabel;
    m_securityLabel = new QLabel;
    for (QLabel* lb : {m_totalLabel, m_securityLabel}) {
        lb->setStyleSheet(QString("color: %1; font-weight: 600;")
                              .arg(FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
        stats->addWidget(lb);
    }
    stats->addStretch();
    layout->addLayout(stats);

    auto* toolbar = new QHBoxLayout;
    m_filterCombo = new QComboBox;
    m_filterCombo->addItem(QStringLiteral("All alerts"), QStringLiteral("all"));
    m_filterCombo->addItem(QStringLiteral("Security"), QStringLiteral("security"));
    m_filterCombo->addItem(QStringLiteral("Warnings"), QStringLiteral("warn"));
    m_filterCombo->addItem(QStringLiteral("Failures"), QStringLiteral("fail"));
    m_filterCombo->addItem(QStringLiteral("Blocked"), QStringLiteral("blocked"));
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AlertsPage::onFilterChanged);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Search alerts…"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AlertsPage::onFilterChanged);

    toolbar->addWidget(new QLabel(QStringLiteral("Show:")));
    toolbar->addWidget(m_filterCombo);
    toolbar->addWidget(m_searchEdit, 1);
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Severity"),
        QStringLiteral("Event"),
        QStringLiteral("Device"),
        QStringLiteral("Type"),
        QStringLiteral("Result"),
        QStringLiteral(""),
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(FSStyle.dataTableStyleSheet());
    layout->addWidget(m_table, 1);

    scroll->setWidget(content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

void AlertsPage::setSummary(int total, int securityCount)
{
    m_totalLabel->setText(QStringLiteral("Showing: %1").arg(total));
    m_securityLabel->setText(QStringLiteral("Security: %1").arg(securityCount));
}

QString AlertsPage::filterKind() const
{
    return m_filterCombo->currentData().toString();
}

QString AlertsPage::searchText() const
{
    return m_searchEdit->text().trimmed();
}

void AlertsPage::setAlerts(const QList<UiEventEntry>& alerts)
{
    m_rows = alerts;
    const QString kind = filterKind();
    const QString needle = searchText().toLower();

    QList<UiEventEntry> visible;
    int securityCount = 0;
    for (const UiEventEntry& e : m_rows) {
        const QString sev = severityLabel(e.result).toLower();
        if (sev == QLatin1String("security")) {
            ++securityCount;
        }

        if (kind == QLatin1String("security")
            && sev != QLatin1String("security") && e.result.toLower() != QLatin1String("alert")) {
            continue;
        }
        if (kind == QLatin1String("warn")
            && sev != QLatin1String("warning") && e.result.toLower() != QLatin1String("warn")) {
            continue;
        }
        if (kind == QLatin1String("fail")) {
            const QString r = e.result.toLower();
            if (r != QLatin1String("fail") && r != QLatin1String("error")
                && r != QLatin1String("mismatch") && r != QLatin1String("failed")) {
                continue;
            }
        }
        if (kind == QLatin1String("blocked")) {
            const QString r = e.result.toLower();
            if (r != QLatin1String("blocked") && r != QLatin1String("rejected")) {
                continue;
            }
        }

        if (!needle.isEmpty()) {
            const QString hay = QStringLiteral("%1 %2 %3 %4 %5")
                                    .arg(e.event, e.device, e.type, e.result, e.detail)
                                    .toLower();
            if (!hay.contains(needle)) {
                continue;
            }
        }
        visible.append(e);
    }

    m_table->setRowCount(visible.size());
    for (int row = 0; row < visible.size(); ++row) {
        const UiEventEntry& e = visible.at(row);
        m_table->setItem(row, 0, readOnlyItem(e.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
        m_table->setItem(row, 1, readOnlyItem(severityLabel(e.result)));
        m_table->setItem(row, 2, readOnlyItem(e.event));
        m_table->setItem(row, 3, readOnlyItem(e.device));
        m_table->setItem(row, 4, readOnlyItem(e.type));
        m_table->setItem(row, 5, readOnlyItem(e.result));

        auto* detailsBtn = new QPushButton(QStringLiteral("Details"));
        detailsBtn->setProperty("alertRow", row);
        connect(detailsBtn, &QPushButton::clicked, this, &AlertsPage::onDetailsClicked);
        m_table->setCellWidget(row, 6, tableButtonCell(detailsBtn));
    }

    m_visibleRows = visible;
    applyTableLayout();
    setSummary(visible.size(), securityCount);
}

void AlertsPage::applyTableLayout()
{
    if (!m_table) {
        return;
    }
    m_table->resizeColumnsToContents();
    const int last = m_table->columnCount() - 1;
    if (last >= 0) {
        m_table->horizontalHeader()->setSectionResizeMode(last, QHeaderView::Fixed);
        m_table->setColumnWidth(last, 72);
    }
    if (m_table->columnCount() >= 3) {
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    }
}

void AlertsPage::onFilterChanged()
{
    setAlerts(m_rows);
    emit filterChanged();
}

void AlertsPage::onDetailsClicked()
{
    const int row = rowIndexForButton(sender());
    if (row < 0 || row >= m_visibleRows.size()) {
        return;
    }
    emit eventDetailsRequested(m_visibleRows.at(row));
}

int AlertsPage::rowIndexForButton(QObject* sender) const
{
    auto* btn = qobject_cast<QPushButton*>(sender);
    if (!btn) {
        return -1;
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->cellWidget(row, 6) && m_table->cellWidget(row, 6)->findChild<QPushButton*>() == btn) {
            return row;
        }
    }
    return btn->property("alertRow").toInt();
}

} // namespace FlashSpartan
