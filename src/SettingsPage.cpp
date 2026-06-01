#include "SettingsPage.h"
#include "SettingsDialog.h"
#include "StyleManager.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace FlashSpartan {

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* title = new QLabel(QStringLiteral("Settings"));
    title->setFont(FSFont(Heading2));
    title->setStyleSheet(QString("color: %1; padding: 20px 24px 0 24px;")
                             .arg(FSStyle.colorCss(StyleManager::ColorRole::TextPrimary)));

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(FSStyle.scrollAreaStyleSheet());

    m_form = new SettingsDialog;
    m_form->setEmbeddedMode(true);
    m_form->setParent(scroll);
    scroll->setWidget(m_form);

    auto* bar = new QWidget;
    auto* barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(24, 12, 24, 20);
    auto* restoreBtn = new QPushButton(QStringLiteral("Restore defaults"));
    restoreBtn->setCursor(Qt::PointingHandCursor);
    restoreBtn->setStyleSheet(FSStyle.buttonStyleSheet());
    connect(restoreBtn, &QPushButton::clicked, m_form, &SettingsDialog::restoreDefaultsTriggered);

    auto* saveBtn = new QPushButton(QStringLiteral("Save changes"));
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        emit settingsApplyRequested(m_form->getSettings());
    });

    barLayout->addWidget(restoreBtn);
    barLayout->addStretch();
    barLayout->addWidget(saveBtn);

    auto* outer = new QWidget;
    auto* outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(title);
    outerLayout->addWidget(scroll, 1);
    outerLayout->addWidget(bar);

    root->addWidget(outer);

    connect(m_form, &SettingsDialog::themeChanged, this, &SettingsPage::themeChanged);
    connect(m_form, &SettingsDialog::liveSettingsChanged, this, &SettingsPage::liveSettingsChanged);
    connect(m_form, &SettingsDialog::exportDatabaseRequested, this,
            &SettingsPage::exportDatabaseRequested);
    connect(m_form, &SettingsDialog::importDatabaseRequested, this,
            &SettingsPage::importDatabaseRequested);
    connect(m_form, &SettingsDialog::backupDatabaseRequested, this,
            &SettingsPage::backupDatabaseRequested);
    connect(m_form, &SettingsDialog::clearDatabaseRequested, this,
            &SettingsPage::clearDatabaseRequested);
}

void SettingsPage::loadSettings(const AppSettings& settings)
{
    m_form->loadSettings(settings);
}

AppSettings SettingsPage::currentSettings() const
{
    return m_form->getSettings();
}

void SettingsPage::setDatabaseStatistics(int deviceCount, const QString& databasePath)
{
    m_form->setDatabaseStatistics(deviceCount, databasePath);
}

} // namespace FlashSpartan
