#include "UsbMonitorPage.h"
#include "StyleManager.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace FlashSentry {

namespace {

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

UsbMonitorPage::UsbMonitorPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("UsbMonitorPage"));
    setupUi();
    styleTables();
}

void UsbMonitorPage::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(20);

    auto* pageTitle = new QLabel(QStringLiteral("USB Monitor"));
    pageTitle->setFont(FSFont(Heading2));
    pageTitle->setStyleSheet(QString("color: %1;")
                                .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    layout->addWidget(pageTitle);

    auto* cards = new QGridLayout;
    cards->setSpacing(12);
    cards->addWidget(createStatCard(QStringLiteral("Connected Devices"), m_connectedValue), 0, 0);
    cards->addWidget(createStatCard(QStringLiteral("Allowed"), m_allowedValue), 0, 1);
    cards->addWidget(createStatCard(QStringLiteral("Blocked"), m_blockedValue), 0, 2);
    cards->addWidget(createStatCard(QStringLiteral("Events"), m_eventsValue), 0, 3);
    for (int c = 0; c < 4; ++c) {
        cards->setColumnStretch(c, 1);
    }
    layout->addLayout(cards);

    auto* devicesLabel = new QLabel(QStringLiteral("Connected devices"));
    devicesLabel->setFont(FSFont(Heading3));
    devicesLabel->setStyleSheet(QString("color: %1;")
                                    .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(devicesLabel);

    m_deviceTable = new QTableWidget(0, 8);
    m_deviceTable->setHorizontalHeaderLabels({
        QStringLiteral("Device name"),
        QStringLiteral("Type"),
        QStringLiteral("Status"),
        QStringLiteral("Capacity"),
        QStringLiteral("Vendor / model"),
        QStringLiteral("Connected"),
        QStringLiteral("Disconnected"),
        QStringLiteral("Actions"),
    });
    m_deviceTable->horizontalHeader()->setStretchLastSection(false);
    m_deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deviceTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_deviceTable->setAlternatingRowColors(true);
  connect(m_deviceTable, &QTableWidget::cellChanged, this, &UsbMonitorPage::onDeviceCellChanged);
    layout->addWidget(m_deviceTable);

    auto* eventsLabel = new QLabel(QStringLiteral("Recent events"));
    eventsLabel->setFont(FSFont(Heading3));
    eventsLabel->setStyleSheet(QString("color: %1;")
                                   .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(eventsLabel);

    m_eventsTable = new QTableWidget(0, 6);
    m_eventsTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Event"),
        QStringLiteral("Device"),
        QStringLiteral("Type"),
        QStringLiteral("Result"),
        QStringLiteral(""),
    });
    m_eventsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_eventsTable->verticalHeader()->setVisible(false);
    m_eventsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventsTable->setAlternatingRowColors(true);
    layout->addWidget(m_eventsTable, 1);

    layout->addStretch();
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

QWidget* UsbMonitorPage::createStatCard(const QString& title, QLabel*& valueLabel)
{
    auto* card = new QWidget;
    card->setObjectName(QStringLiteral("StatCard"));
    card->setMinimumHeight(88);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    auto* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 12px;")
                                  .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    valueLabel = new QLabel(QStringLiteral("0"));
    valueLabel->setFont(FSFont(Heading1));
    valueLabel->setStyleSheet(QString("color: %1; font-weight: 700;")
                                  .arg(FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    card->setStyleSheet(QString(R"(
        QWidget#StatCard {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }
    )")
                            .arg(FSStyle.colorCss(StyleManager::ColorRole::Surface),
                                 FSStyle.colorCss(StyleManager::ColorRole::Border)));
    return card;
}

void UsbMonitorPage::styleTables()
{
    const QString tableCss = FSStyle.dataTableStyleSheet();
    m_deviceTable->setStyleSheet(tableCss);
    m_eventsTable->setStyleSheet(tableCss);
}

void UsbMonitorPage::setStats(const UsbMonitorStats& stats)
{
    m_connectedValue->setText(QString::number(stats.connected));
    m_allowedValue->setText(QString::number(stats.allowed));
    m_blockedValue->setText(QString::number(stats.blocked));
    m_eventsValue->setText(QString::number(stats.events));
}

void UsbMonitorPage::setDevices(const QList<UsbDeviceRow>& rows)
{
    m_deviceRows = rows;
    m_blockDeviceTableSignals = true;
    m_deviceTable->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const UsbDeviceRow& row = rows.at(r);
        auto* nameItem = new QTableWidgetItem(row.displayName);
        nameItem->setData(Qt::UserRole, row.deviceNode);
        if (!row.nameEditable) {
            nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        }
        m_deviceTable->setItem(r, 0, nameItem);
        m_deviceTable->setItem(r, 1, readOnlyItem(row.type));
        m_deviceTable->setItem(r, 2, readOnlyItem(row.status));
        m_deviceTable->setItem(r, 3, readOnlyItem(row.capacity));
        m_deviceTable->setItem(r, 4, readOnlyItem(row.vendorModel));
        m_deviceTable->setItem(r, 5, readOnlyItem(row.connectedAt));
        m_deviceTable->setItem(r, 6, readOnlyItem(row.disconnectedAt));

        auto* actionsWidget = new QWidget;
        auto* actionsLayout = new QHBoxLayout(actionsWidget);
        actionsLayout->setContentsMargins(4, 2, 4, 2);
        auto* actionsBtn = new QPushButton(QStringLiteral("Actions"));
        actionsBtn->setProperty("deviceNode", row.deviceNode);
        actionsBtn->setCursor(Qt::PointingHandCursor);
        actionsBtn->setStyleSheet(FSStyle.buttonStyleSheet());
        connect(actionsBtn, &QPushButton::clicked, this, &UsbMonitorPage::onDeviceActionsClicked);
        actionsLayout->addWidget(actionsBtn);
        m_deviceTable->setCellWidget(r, 7, actionsWidget);
        m_deviceTable->setRowHeight(r, 40);
    }
    m_blockDeviceTableSignals = false;
}

void UsbMonitorPage::setEvents(const QList<UiEventEntry>& events)
{
    m_eventRows = events;
    m_eventsTable->setRowCount(events.size());
    for (int r = 0; r < events.size(); ++r) {
        const UiEventEntry& e = events.at(r);
        m_eventsTable->setItem(r, 0, readOnlyItem(e.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
        m_eventsTable->setItem(r, 1, readOnlyItem(e.event));
        m_eventsTable->setItem(r, 2, readOnlyItem(e.device));
        m_eventsTable->setItem(r, 3, readOnlyItem(e.type));
        m_eventsTable->setItem(r, 4, readOnlyItem(e.result));

        auto* detailsWidget = new QWidget;
        auto* detailsLayout = new QHBoxLayout(detailsWidget);
        detailsLayout->setContentsMargins(4, 2, 4, 2);
        auto* detailsBtn = new QPushButton(QStringLiteral("Details"));
        detailsBtn->setProperty("eventRow", r);
        detailsBtn->setCursor(Qt::PointingHandCursor);
        detailsBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
        connect(detailsBtn, &QPushButton::clicked, this, &UsbMonitorPage::onEventDetailsClicked);
        detailsLayout->addWidget(detailsBtn);
        m_eventsTable->setCellWidget(r, 5, detailsWidget);
        m_eventsTable->setRowHeight(r, 36);
    }
}

void UsbMonitorPage::onDeviceCellChanged(int row, int column)
{
    if (m_blockDeviceTableSignals || column != 0 || row < 0 || row >= m_deviceRows.size()) {
        return;
    }
    QTableWidgetItem* item = m_deviceTable->item(row, 0);
    if (!item) {
        return;
    }
    const QString node = item->data(Qt::UserRole).toString();
    emit deviceNameEdited(node, item->text().trimmed());
}

void UsbMonitorPage::onDeviceActionsClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    emit deviceActionsRequested(btn->property("deviceNode").toString());
}

void UsbMonitorPage::onEventDetailsClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    const int row = btn->property("eventRow").toInt();
    if (row < 0 || row >= m_eventRows.size()) {
        return;
    }
    emit eventDetailsRequested(m_eventRows.at(row));
}

} // namespace FlashSentry
