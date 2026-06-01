#include "AboutPage.h"
#include "Platform.h"
#include "StyleManager.h"
#include "UiIcons.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace FlashSpartan {

AboutPage::AboutPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void AboutPage::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 32, 24, 32);
    layout->setSpacing(16);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* logo = new QLabel;
    UiIcons::setLabelPixmap(logo, ":/branding/flashspartan-logo.png", 96);
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo, 0, Qt::AlignHCenter);

    auto* name = new QLabel(QStringLiteral("FlashSpartan"));
    name->setFont(FSFont(Heading1));
    name->setAlignment(Qt::AlignCenter);
    name->setStyleSheet(QString("color: %1;")
                            .arg(FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    layout->addWidget(name);

    m_versionLabel = new QLabel;
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_versionLabel->setStyleSheet(QString("color: %1;")
                                      .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(m_versionLabel);

    const QString platformBlurb =
        Platform::isWindows()
            ? QStringLiteral(
                  "USB flash drive security monitor for Windows.\n\n"
                  "Cryptographic verification, allow/block policy, ISO checks, and BadUSB "
                  "HID monitoring — with optional USBPcap packet capture.")
            : QStringLiteral(
                  "USB flash drive security monitor for Linux.\n\n"
                  "Cryptographic verification, allow/block policy, ISO checks, and BadUSB "
                  "HID monitoring — built for speed and clarity on Arch-based systems.");
    auto* desc = new QLabel(platformBlurb);
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    desc->setMaximumWidth(520);
    desc->setStyleSheet(QString("color: %1;")
                            .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(desc);

    auto* links = new QHBoxLayout;
    links->setAlignment(Qt::AlignCenter);
    links->setSpacing(16);

    auto* githubBtn = new QPushButton(QStringLiteral("GitHub"));
    githubBtn->setCursor(Qt::PointingHandCursor);
    connect(githubBtn, &QPushButton::clicked, this, &AboutPage::openRepositoryRequested);

    auto* guideBtn = new QPushButton(QStringLiteral("User guide"));
    guideBtn->setCursor(Qt::PointingHandCursor);
    connect(guideBtn, &QPushButton::clicked, this, &AboutPage::openUserGuideRequested);

    links->addWidget(githubBtn);
    links->addWidget(guideBtn);
    layout->addLayout(links);

    m_dbLabel = new QLabel;
    m_dbLabel->setAlignment(Qt::AlignCenter);
    m_dbLabel->setWordWrap(true);
    m_dbLabel->setMaximumWidth(560);
    m_dbLabel->setStyleSheet(QString("color: %1; font-size: 10pt;")
                                 .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(m_dbLabel);

    m_runtimeLabel = new QLabel;
    m_runtimeLabel->setAlignment(Qt::AlignCenter);
    m_runtimeLabel->setStyleSheet(QString("color: %1; font-size: 9pt;")
                                      .arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    layout->addWidget(m_runtimeLabel);

    layout->addStretch();

    scroll->setWidget(content);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

void AboutPage::setVersion(const QString& version)
{
    m_versionLabel->setText(QStringLiteral("Version %1").arg(version));
}

void AboutPage::setDatabaseSummary(int deviceCount, const QString& storePath)
{
    m_dbLabel->setText(
        QStringLiteral("Trusted devices in policy store: %1\n%2").arg(deviceCount).arg(storePath));
}

void AboutPage::setRuntimeInfo(const QString& qtVersion, const QString& osName)
{
    m_runtimeLabel->setText(QStringLiteral("Qt %1 · %2 · C++20").arg(qtVersion, osName));
}

} // namespace FlashSpartan
