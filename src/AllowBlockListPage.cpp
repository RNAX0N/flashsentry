#include "AllowBlockListPage.h"
#include "StyleManager.h"
#include "UiIcons.h"

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

QWidget* compactButton(QPushButton* btn)
{
    btn->setFixedHeight(20);
    btn->setStyleSheet(FSStyle.compactTableButtonStyleSheet());
    auto* cell = new QWidget;
    auto* lay = new QHBoxLayout(cell);
    lay->setContentsMargins(2, 1, 2, 1);
    lay->addWidget(btn, 0, Qt::AlignCenter);
    return cell;
}

} // namespace

AllowBlockListPage::AllowBlockListPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AllowBlockListPage::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Allow/Block List"));
    title->setFont(FSFont(Heading2));
    title->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    layout->addWidget(title);

    auto* intro = new QLabel(QStringLiteral(
        "Manage trusted and blocked USB drives. Blocked devices stay blocked after restart."));
    intro->setWordWrap(true);
    intro->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(intro);

    auto* stats = new QHBoxLayout;
    m_allowedLabel = new QLabel;
    m_blockedLabel = new QLabel;
    m_totalLabel = new QLabel;
    for (QLabel* lb : {m_allowedLabel, m_blockedLabel, m_totalLabel}) {
        lb->setStyleSheet(QString("color: %1; font-weight: 600;")
                              .arg(FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
        stats->addWidget(lb);
    }
    stats->addStretch();
    layout->addLayout(stats);

    auto* toolbar = new QHBoxLayout;
    m_filterCombo = new QComboBox;
    m_filterCombo->addItem(QStringLiteral("All devices"), QStringLiteral("all"));
    m_filterCombo->addItem(QStringLiteral("Allowed"), QStringLiteral("allowed"));
    m_filterCombo->addItem(QStringLiteral("Blocked"), QStringLiteral("blocked"));
    m_filterCombo->addItem(QStringLiteral("Unknown"), QStringLiteral("unknown"));
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AllowBlockListPage::onFilterChanged);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Search name, serial, vendor…"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AllowBlockListPage::onFilterChanged);

    toolbar->addWidget(new QLabel(QStringLiteral("Show:")));
    toolbar->addWidget(m_filterCombo);
    toolbar->addWidget(m_searchEdit, 1);
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, 7);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Device"),
        QStringLiteral("Status"),
        QStringLiteral("Trust"),
        QStringLiteral("Vendor / model"),
        QStringLiteral("Connected"),
        QStringLiteral("History"),
        QStringLiteral("Actions"),
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

QString AllowBlockListPage::currentFilterId() const
{
    return m_filterCombo ? m_filterCombo->currentData().toString() : QStringLiteral("all");
}

QString AllowBlockListPage::searchText() const
{
    return m_searchEdit ? m_searchEdit->text().trimmed() : QString();
}

void AllowBlockListPage::setSummary(int allowed, int blocked, int total)
{
    m_allowedLabel->setText(QStringLiteral("Allowed: %1").arg(allowed));
    m_blockedLabel->setText(QStringLiteral("Blocked: %1").arg(blocked));
    m_totalLabel->setText(QStringLiteral("Listed: %1").arg(total));
}

void AllowBlockListPage::setRows(const QList<AllowBlockRow>& rows)
{
    m_rows = rows;
    m_table->setRowCount(rows.size());
    const QIcon driveIcon = UiIcons::icon(":/icons/usb-drive.svg", 18);

    for (int r = 0; r < rows.size(); ++r) {
        const AllowBlockRow& row = rows.at(r);
        auto* nameItem = new QTableWidgetItem(row.displayName);
        nameItem->setIcon(driveIcon);
        nameItem->setData(Qt::UserRole, r);
        m_table->setItem(r, 0, nameItem);
        m_table->setItem(r, 1, readOnlyItem(row.status));
        m_table->setItem(r, 2, readOnlyItem(row.trustDetail));
        m_table->setItem(r, 3, readOnlyItem(row.vendorModel));
        m_table->setItem(r, 4, readOnlyItem(row.isConnected ? QStringLiteral("Yes")
                                                              : QStringLiteral("No")));

        if (!row.deviceNode.isEmpty()) {
            auto* histBtn = new QPushButton(QStringLiteral("History"));
            histBtn->setProperty("row", r);
            histBtn->setCursor(Qt::PointingHandCursor);
            connect(histBtn, &QPushButton::clicked, this, &AllowBlockListPage::onHistoryClicked);
            m_table->setCellWidget(r, 5, compactButton(histBtn));
        }

        auto* actions = new QWidget;
        auto* actionsLay = new QHBoxLayout(actions);
        actionsLay->setContentsMargins(2, 1, 2, 1);
        actionsLay->setSpacing(4);

        if (row.isBlocked) {
            auto* unbtn = new QPushButton(QStringLiteral("Unblock"));
            unbtn->setProperty("row", r);
            unbtn->setCursor(Qt::PointingHandCursor);
            connect(unbtn, &QPushButton::clicked, this, &AllowBlockListPage::onUnblockClicked);
            actionsLay->addWidget(unbtn);
        } else {
            auto* allowBtn = new QPushButton(QStringLiteral("Allow"));
            allowBtn->setProperty("row", r);
            allowBtn->setCursor(Qt::PointingHandCursor);
            connect(allowBtn, &QPushButton::clicked, this, &AllowBlockListPage::onAllowClicked);
            actionsLay->addWidget(allowBtn);

            auto* blockBtn = new QPushButton(QStringLiteral("Block"));
            blockBtn->setProperty("row", r);
            blockBtn->setCursor(Qt::PointingHandCursor);
            connect(blockBtn, &QPushButton::clicked, this, &AllowBlockListPage::onBlockClicked);
            actionsLay->addWidget(blockBtn);
        }

        if (!row.uniqueId.isEmpty()) {
            auto* remBtn = new QPushButton(QStringLiteral("Remove"));
            remBtn->setProperty("row", r);
            remBtn->setCursor(Qt::PointingHandCursor);
            connect(remBtn, &QPushButton::clicked, this, &AllowBlockListPage::onRemoveClicked);
            actionsLay->addWidget(remBtn);
        }

        for (auto* btn : actions->findChildren<QPushButton*>()) {
            btn->setFixedHeight(20);
            btn->setStyleSheet(FSStyle.compactTableButtonStyleSheet());
        }
        m_table->setCellWidget(r, 6, actions);
        m_table->setRowHeight(r, 32);
    }
    applyTableLayout();
}

void AllowBlockListPage::applyTableLayout()
{
    auto* hdr = m_table->horizontalHeader();
    hdr->setStretchLastSection(false);
    hdr->setSectionResizeMode(0, QHeaderView::Interactive);
    hdr->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(3, QHeaderView::Stretch);
    hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(5, QHeaderView::Fixed);
    hdr->setSectionResizeMode(6, QHeaderView::Fixed);
    m_table->setColumnWidth(5, 76);
    m_table->setColumnWidth(6, 200);

    const QFontMetrics fm(m_table->font());
    m_table->setColumnWidth(0, qMin(fm.horizontalAdvance(QStringLiteral("SanDisk Ultra")) + 48, 200));
}

int AllowBlockListPage::rowIndexForButton(QObject* sender) const
{
    auto* btn = qobject_cast<QPushButton*>(sender);
    if (!btn) {
        return -1;
    }
    return btn->property("row").toInt();
}

void AllowBlockListPage::onAllowClicked()
{
    const int r = rowIndexForButton(sender());
    if (r < 0 || r >= m_rows.size()) {
        return;
    }
    const AllowBlockRow& row = m_rows.at(r);
    emit allowRequested(row.uniqueId, row.driveKey);
}

void AllowBlockListPage::onBlockClicked()
{
    const int r = rowIndexForButton(sender());
    if (r < 0 || r >= m_rows.size()) {
        return;
    }
    const AllowBlockRow& row = m_rows.at(r);
    emit blockRequested(row.uniqueId, row.driveKey, row.displayName);
}

void AllowBlockListPage::onUnblockClicked()
{
    const int r = rowIndexForButton(sender());
    if (r < 0 || r >= m_rows.size()) {
        return;
    }
    const AllowBlockRow& row = m_rows.at(r);
    emit unblockRequested(row.uniqueId, row.driveKey);
}

void AllowBlockListPage::onRemoveClicked()
{
    const int r = rowIndexForButton(sender());
    if (r < 0 || r >= m_rows.size()) {
        return;
    }
    emit removeFromWhitelistRequested(m_rows.at(r).uniqueId);
}

void AllowBlockListPage::onHistoryClicked()
{
    const int r = rowIndexForButton(sender());
    if (r < 0 || r >= m_rows.size()) {
        return;
    }
    const QString node = m_rows.at(r).deviceNode;
    if (!node.isEmpty()) {
        emit historyRequested(node);
    }
}

void AllowBlockListPage::onFilterChanged()
{
    emit filterChanged();
}

} // namespace FlashSpartan
