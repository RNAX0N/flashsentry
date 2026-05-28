#include "WelcomeWizard.h"
#include "SettingsProfiles.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QVBoxLayout>
#include <QWizardPage>

namespace FlashSentry {

namespace {

bool userInStorageGroup()
{
    QProcess proc;
    proc.start(QStringLiteral("id"), {QStringLiteral("-Gn")});
    if (!proc.waitForFinished(3000)) {
        return false;
    }
    const QStringList groups = QString::fromUtf8(proc.readAllStandardOutput()).split(QLatin1Char(' '),
                                                                                   Qt::SkipEmptyParts);
    return groups.contains(QStringLiteral("storage"));
}

class IntroPage : public QWizardPage {
public:
    IntroPage()
    {
        setTitle(QStringLiteral("Welcome to FlashSentry"));
        setSubTitle(QStringLiteral("Verify USB sticks and image files on Arch Linux"));

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(QStringLiteral(
            "<p>FlashSentry helps you trust removable USB storage:</p>"
            "<ul>"
            "<li><b>ISO / image verify</b> — checksums and signatures for Linux ISOs, Windows "
            "installers, Raspberry Pi images, and files copied with <code>dd</code>, Rufus, or "
            "any multiboot layout.</li>"
            "<li><b>Watch folders</b> — fast Merkle baselines for selected directories.</li>"
            "<li><b>Full partition hash</b> — optional byte-for-byte scan.</li>"
            "</ul>"
            "<p>This wizard sets a security preset and checks a few system items.</p>")));
    }
};

class ProfilePage : public QWizardPage {
public:
    ProfilePage(QComboBox** comboOut)
    {
        setTitle(QStringLiteral("Security preset"));
        setSubTitle(QStringLiteral("You can change this anytime in Settings"));

        auto* layout = new QVBoxLayout(this);
        m_combo = new QComboBox;
        for (const QString& id : SettingsProfiles::profileIds()) {
            m_combo->addItem(SettingsProfiles::profileDisplayName(id), id);
        }
        m_combo->setCurrentIndex(m_combo->findData(QStringLiteral("default")));
        layout->addWidget(m_combo);

        m_description = new QLabel;
        m_description->setWordWrap(true);
        layout->addWidget(m_description);

        connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &ProfilePage::updateDescription);
        updateDescription(m_combo->currentIndex());

        if (comboOut) {
            *comboOut = m_combo;
        }
    }

    QString profileId() const
    {
        return SettingsProfiles::normalizeProfileId(m_combo->currentData().toString());
    }

private:
    void updateDescription(int index)
    {
        const QString id = SettingsProfiles::normalizeProfileId(m_combo->itemData(index).toString());
        m_description->setText(SettingsProfiles::profileDescription(id));
    }

    QComboBox* m_combo = nullptr;
    QLabel* m_description = nullptr;
};

class SystemPage : public QWizardPage {
public:
    SystemPage()
    {
        setTitle(QStringLiteral("System setup"));
        setSubTitle(QStringLiteral("Recommended once per machine"));

        auto* layout = new QVBoxLayout(this);

        const bool inStorage = userInStorageGroup();
        m_storageOk = new QLabel(inStorage
                                     ? QStringLiteral("✓ Your user is in the <b>storage</b> group.")
                                     : QStringLiteral(
                                           "⚠ Add yourself to the <b>storage</b> group for raw "
                                           "device reads, then log out and back in:<br>"
                                           "<code>sudo usermod -aG storage $USER</code>"));
        m_storageOk->setWordWrap(true);
        layout->addWidget(m_storageOk);

        auto* automountBox = new QGroupBox(QStringLiteral("Disable desktop auto-mount (recommended)"));
        auto* automountLayout = new QVBoxLayout(automountBox);
        automountLayout->addWidget(new QLabel(QStringLiteral(
            "Let FlashSentry mount after verification. GNOME example:")));
        auto* cmd = new QPlainTextEdit(QStringLiteral(
            "gsettings set org.gnome.desktop.media-handling automount false\n"
            "gsettings set org.gnome.desktop.media-handling automount-open false"));
        cmd->setReadOnly(true);
        cmd->setMaximumHeight(72);
        automountLayout->addWidget(cmd);
        automountLayout->addWidget(new QLabel(QStringLiteral(
            "KDE: System Settings → Removable Storage → disable automatic mounting.")));
        layout->addWidget(automountBox);

        m_skipWizard = new QCheckBox(QStringLiteral("Show this wizard again on next start"));
        m_skipWizard->setChecked(false);
        layout->addWidget(m_skipWizard);
    }

    bool showWizardAgain() const { return m_skipWizard->isChecked(); }

private:
    QLabel* m_storageOk = nullptr;
    QCheckBox* m_skipWizard = nullptr;
};

} // namespace

WelcomeWizard::WelcomeWizard(QWidget* parent)
    : QWizard(parent)
{
    setWindowTitle(QStringLiteral("FlashSentry setup"));
    setMinimumSize(520, 420);
    setWizardStyle(QWizard::ModernStyle);
    setupPages();
}

void WelcomeWizard::setupPages()
{
    addPage(new IntroPage);

    QComboBox* profileCombo = nullptr;
    addPage(new ProfilePage(&profileCombo));

    auto* systemPage = new SystemPage;
    addPage(systemPage);

    connect(this, &QWizard::accepted, this, [this, profileCombo, systemPage]() {
        if (profileCombo) {
            setProperty("selectedProfile", profileCombo->currentData());
        }
        setProperty("showWizardAgain", systemPage->showWizardAgain());
    });
}

QString WelcomeWizard::selectedProfileId() const
{
    return SettingsProfiles::normalizeProfileId(property("selectedProfile").toString());
}

void WelcomeWizard::applyToSettings(AppSettings& settings) const
{
    const QString profile = selectedProfileId();
    SettingsProfiles::applyProfile(profile, settings);
    settings.showFirstRunWizard = property("showWizardAgain").toBool();
}

} // namespace FlashSentry
