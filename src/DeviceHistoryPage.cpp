#include "DeviceHistoryPage.h"
#include "StyleManager.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
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
    btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    btn->setStyleSheet(FSStyle.compactTableButtonStyleSheet());
    auto* cell = new QWidget;
    auto* lay = new QHBoxLayout(cell);
    lay->setContentsMargins(2, 1, 2, 1);
    lay->addWidget(btn, 0, Qt::AlignCenter);
    return cell;
}

} // namespace

DeviceHistoryPage::DeviceHistoryPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DeviceHistoryPage::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Device History"));
    title->setFont(FSFont(Heading2));
    title->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));
    layout->addWidget(title);

    auto* picker = new QFormLayout;
    m_deviceCombo = new QComboBox;
    m_deviceCombo->setMinimumWidth(320);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DeviceHistoryPage::onDeviceComboChanged);
    picker->addRow(QStringLiteral("Device:"), m_deviceCombo);
    layout->addLayout(picker);

    m_eventsTable = new QTableWidget(0, 6);
    m_eventsTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Event"),
        QStringLiteral("Device"),
        QStringLiteral("Type"),
        QStringLiteral("Result"),
        QStringLiteral(""),
    });
    m_eventsTable->verticalHeader()->setVisible(false);
    m_eventsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventsTable->setAlternatingRowColors(true);
    m_eventsTable->setStyleSheet(FSStyle.dataTableStyleSheet());
    layout->addWidget(m_eventsTable, 1);

    scroll->setWidget(content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

void DeviceHistoryPage::setDeviceChoices(const QStringList& labels, const QStringList& deviceNodes)
{
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    for (int i = 0; i < labels.size() && i < deviceNodes.size(); ++i) {
        m_deviceCombo->addItem(labels.at(i), deviceNodes.at(i));
    }
    m_deviceCombo->blockSignals(false);
}

void DeviceHistoryPage::setSelectedDevice(const QString& deviceNode)
{
    const int idx = m_deviceCombo->findData(deviceNode);
    if (idx >= 0) {
        m_deviceCombo->setCurrentIndex(idx);
    }
}

QString DeviceHistoryPage::selectedDeviceNode() const
{
    return m_deviceCombo->currentData().toString();
}

void DeviceHistoryPage::setEvents(const QList<UiEventEntry>& events)
{
    m_eventRows = events;
    m_eventsTable->setRowCount(events.size());
    const QString timeFmt = QStringLiteral("yyyy-MM-dd hh:mm:ss");
    for (int r = 0; r < events.size(); ++r) {
        const UiEventEntry& e = events.at(r);
        m_eventsTable->setItem(r, 0, readOnlyItem(e.time.toString(timeFmt)));
        m_eventsTable->setItem(r, 1, readOnlyItem(e.event));
        m_eventsTable->setItem(r, 2, readOnlyItem(e.device));
        m_eventsTable->setItem(r, 3, readOnlyItem(e.type));
        m_eventsTable->setItem(r, 4, readOnlyItem(e.result));

        auto* detailsBtn = new QPushButton(QStringLiteral("Details"));
        detailsBtn->setProperty("eventRow", r);
        detailsBtn->setCursor(Qt::PointingHandCursor);
        connect(detailsBtn, &QPushButton::clicked, this, &DeviceHistoryPage::onEventDetailsClicked);
        m_eventsTable->setCellWidget(r, 5, tableButtonCell(detailsBtn));
        m_eventsTable->setRowHeight(r, 30);
    }
    applyTableLayout();
}

void DeviceHistoryPage::applyTableLayout()
{
    auto* hdr = m_eventsTable->horizontalHeader();
    hdr->setStretchLastSection(false);
    hdr->setSectionResizeMode(1, QHeaderView::Stretch);
    hdr->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    hdr->setSectionResizeMode(5, QHeaderView::Fixed);
    m_eventsTable->setColumnWidth(5, 88);

    const QFontMetrics fm(m_eventsTable->font());
    const int minTime = fm.horizontalAdvance(QStringLiteral("2026-05-28 12:34:56")) + 28;
    hdr->setSectionResizeMode(0, QHeaderView::Interactive);
    m_eventsTable->setColumnWidth(0, qMax(minTime, m_eventsTable->columnWidth(0)));
    if (m_eventsTable->columnWidth(0) < minTime * 2) {
        m_eventsTable->setColumnWidth(0, minTime * 2);
    }
}

void DeviceHistoryPage::onDeviceComboChanged(int index)
{
    if (index < 0) {
        return;
    }
    emit deviceSelectionChanged(m_deviceCombo->itemData(index).toString());
}

void DeviceHistoryPage::onEventDetailsClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    const int row = btn->property("eventRow").toInt();
    if (row >= 0 && row < m_eventRows.size()) {
        emit eventDetailsRequested(m_eventRows.at(row));
    }
}

} // namespace FlashSpartan
