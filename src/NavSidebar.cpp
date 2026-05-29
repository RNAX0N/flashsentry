#include "NavSidebar.h"
#include "StyleManager.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QFrame>

namespace FlashSentry {

NavSidebar::NavSidebar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("NavSidebar"));
    setFixedWidth(200);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* brand = new QWidget;
    brand->setObjectName(QStringLiteral("NavBrand"));
    auto* brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(16, 20, 16, 16);
    auto* title = new QLabel(QStringLiteral("FlashSentry"));
    title->setFont(FSFont(Heading3));
    title->setStyleSheet(QString("color: %1; font-weight: 700;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    brandLayout->addWidget(title);
    auto* sub = new QLabel(QStringLiteral("USB Security"));
    sub->setStyleSheet(QString("color: %1;")
                           .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    brandLayout->addWidget(sub);
    outer->addWidget(brand);

    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("color: %1;")
                            .arg(FSStyle.colorCss(StyleManager::ColorRole::Border)));
    outer->addWidget(line);

    m_layout = new QVBoxLayout;
    m_layout->setContentsMargins(8, 12, 8, 12);
    m_layout->setSpacing(4);
    outer->addLayout(m_layout, 1);

    rebuildButtons();

    setStyleSheet(QString(R"(
        QWidget#NavSidebar {
            background-color: %1;
            border-right: 1px solid %2;
        }
        QWidget#NavBrand {
            background-color: transparent;
        }
        QPushButton[navItem="true"] {
            text-align: left;
            padding: 10px 14px;
            border: none;
            border-radius: 8px;
            color: %3;
            background-color: transparent;
        }
        QPushButton[navItem="true"]:hover {
            background-color: %4;
            color: %5;
        }
        QPushButton[navItem="true"]:checked {
            background-color: %6;
            color: %7;
            font-weight: 600;
        }
    )")
                        .arg(FSStyle.colorCss(StyleManager::ColorRole::BackgroundAlt),
                             FSStyle.colorCss(StyleManager::ColorRole::Border),
                             FSStyle.colorCss(StyleManager::ColorRole::TextSecondary),
                             FSStyle.colorCss(StyleManager::ColorRole::SurfaceHover),
                             FSStyle.colorCss(StyleManager::ColorRole::TextPrimary),
                             FSStyle.colorCss(StyleManager::ColorRole::Surface),
                             FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
}

void NavSidebar::rebuildButtons()
{
    for (QPushButton* btn : m_buttons) {
        m_layout->removeWidget(btn);
        btn->deleteLater();
    }
    m_buttons.clear();

    const QList<AppPage> pages = {
        AppPage::UsbMonitor,     AppPage::DeviceHistory, AppPage::AllowBlockList,
        AppPage::Alerts,         AppPage::Reports,     AppPage::Settings,
        AppPage::About,
    };

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    for (AppPage page : pages) {
        auto* btn = new QPushButton(appPageLabel(page));
        btn->setProperty("navItem", true);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("appPage", static_cast<int>(page));
        connect(btn, &QPushButton::clicked, this, &NavSidebar::onButtonClicked);
        group->addButton(btn);
        m_layout->addWidget(btn);
        m_buttons.append(btn);
        if (page == m_current) {
            btn->setChecked(true);
        }
    }
    m_layout->addStretch();
}

void NavSidebar::setCurrentPage(AppPage page)
{
    m_current = page;
    for (QPushButton* btn : m_buttons) {
        const bool match = btn->property("appPage").toInt() == static_cast<int>(page);
        btn->setChecked(match);
    }
}

void NavSidebar::onButtonClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    const auto page = static_cast<AppPage>(btn->property("appPage").toInt());
    m_current = page;
    emit pageSelected(page);
}

} // namespace FlashSentry
