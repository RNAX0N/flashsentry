#include "WatchListsPanel.h"

#include "StyleManager.h"
#include "Types.h"
#include "UiIcons.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

namespace FlashSentry {

WatchListsPanel::WatchListsPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("WatchListsPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("Watch Lists"));
    title->setFont(FSFont(Heading3));
    title->setStyleSheet(QString("color: %1;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(title);

    m_hintLabel = new QLabel(
        QStringLiteral("Select a mounted USB device to define Merkle watch groups and build a fast baseline. "
                       "Changes to watched files are detected without hashing the whole drive."));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QString("color: %1;")
                                   .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    layout->addWidget(m_hintLabel);

    m_deviceList = new QListWidget;
    m_deviceList->setFrameShape(QFrame::NoFrame);
    m_deviceList->setStyleSheet(FSStyle.listWidgetStyleSheet());
    m_deviceList->setSpacing(4);
    layout->addWidget(m_deviceList, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_editBtn = new QPushButton(QStringLiteral("Edit watch lists…"));
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    m_editBtn->setEnabled(false);
    UiIcons::setButtonIcon(m_editBtn, ":/icons/folder-open.svg", 18);
    connect(m_editBtn, &QPushButton::clicked, this, &WatchListsPanel::onEditClicked);
    connect(m_deviceList, &QListWidget::itemSelectionChanged, this,
            &WatchListsPanel::onSelectionChanged);
    btnRow->addWidget(m_editBtn);
    layout->addLayout(btnRow);
}

void WatchListsPanel::refresh(const QList<DeviceInfo>& devices,
                              const QHash<QString, int>& groupCountByDeviceNode)
{
    m_deviceList->clear();
    for (const DeviceInfo& d : devices) {
        if (d.mountPoint.isEmpty()) {
            continue;
        }
        const int groups = groupCountByDeviceNode.value(d.deviceNode, 0);
        const QString line = groups > 0
                                 ? QStringLiteral("%1  —  %2  (%3 group(s))")
                                       .arg(d.displayName(), d.mountPoint)
                                       .arg(groups)
                                 : QStringLiteral("%1  —  %2  (no baseline)")
                                       .arg(d.displayName(), d.mountPoint);
        auto* item = new QListWidgetItem(line);
        item->setData(Qt::UserRole, d.deviceNode);
        m_deviceList->addItem(item);
    }
    if (m_deviceList->count() == 0) {
        m_deviceList->addItem(QStringLiteral("No mounted USB partitions — connect and mount a device first."));
        m_deviceList->item(0)->setFlags(Qt::NoItemFlags);
    }
    onSelectionChanged();
}

void WatchListsPanel::onEditClicked()
{
    auto* item = m_deviceList->currentItem();
    if (!item) {
        return;
    }
    const QString node = item->data(Qt::UserRole).toString();
    if (!node.isEmpty()) {
        emit editDeviceRequested(node);
    }
}

void WatchListsPanel::onSelectionChanged()
{
    auto* item = m_deviceList->currentItem();
    const bool ok = item && !item->data(Qt::UserRole).toString().isEmpty();
    m_editBtn->setEnabled(ok);
}

} // namespace FlashSentry
