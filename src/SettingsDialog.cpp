#include "SettingsDialog.h"
#include "AutostartManager.h"
#include "SettingsProfiles.h"

#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

namespace FlashSentry {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("FlashSentry Settings");
    setMinimumSize(550, 500);
    setModal(true);
    
    // Initialize theme list
    m_themeList = FSStyle.availableThemes();
    
    setupUi();
    applyStyle();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::loadSettings(const AppSettings& settings)
{
    m_blockSignals = true;
    m_originalSettings = settings;
    
    // General
    m_startMinimizedCheck->setChecked(settings.startMinimized);
    if (const auto enabled = AutostartManager::isLoginAutostartEnabled(); enabled.has_value()) {
        m_autoStartCheck->setChecked(*enabled);
    } else {
        m_autoStartCheck->setChecked(settings.autoStartAtLogin);
    }
    m_minimizeToTrayCheck->setChecked(settings.minimizeToTray);
    m_showNotificationsCheck->setChecked(settings.showNotifications);
    
    // Security
    m_autoHashOnConnectCheck->setChecked(settings.autoHashOnConnect);
    m_autoHashOnEjectCheck->setChecked(settings.autoHashOnEject);
    m_confirmNewDeviceCheck->setChecked(settings.requireConfirmationForNew);
    m_confirmModifiedCheck->setChecked(settings.requireConfirmationForModified);
    m_promptPerPartitionCheck->setChecked(settings.promptPerPartition);
    m_blockModifiedCheck->setChecked(settings.blockModifiedDevices);
    for (int i = 0; i < m_appModuleCombo->count(); ++i) {
        if (m_appModuleCombo->itemData(i).toInt() == static_cast<int>(settings.appModule)) {
            m_appModuleCombo->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < m_defaultProfileCombo->count(); ++i) {
        if (m_defaultProfileCombo->itemData(i).toInt() == static_cast<int>(settings.defaultVerificationProfile)) {
            m_defaultProfileCombo->setCurrentIndex(i);
            break;
        }
    }
    m_isoDirEdit->setText(settings.isoScanDirectory);
    m_isoAutoVerifyCheck->setChecked(settings.isoAutoVerifyOnScan);
    if (m_isoAutoVerifyOnUsbMountCheck)
        m_isoAutoVerifyOnUsbMountCheck->setChecked(settings.isoAutoVerifyOnUsbMount);
    if (m_blockMountOnIsoFailCheck)
        m_blockMountOnIsoFailCheck->setChecked(settings.blockMountOnIsoVerifyFailure);
    if (m_isoVerifyDecompressedCheck)
        m_isoVerifyDecompressedCheck->setChecked(settings.isoVerifyDecompressed);
    if (m_isoPreferOfflineCheck)
        m_isoPreferOfflineCheck->setChecked(settings.isoPreferOfflineSidecars);
    if (m_isoParallelSpin)
        m_isoParallelSpin->setValue(settings.isoVerifyParallel);
    if (m_settingsProfileCombo) {
        const QString profileId = SettingsProfiles::normalizeProfileId(settings.settingsProfile);
        const int idx = m_settingsProfileCombo->findData(profileId);
        if (idx >= 0) {
            m_settingsProfileCombo->setCurrentIndex(idx);
        }
    }
    if (m_badUsbEnabledCheck) m_badUsbEnabledCheck->setChecked(settings.badUsbEnabled);
    if (m_badUsbAlertNewKeyboardCheck) m_badUsbAlertNewKeyboardCheck->setChecked(settings.badUsbAlertNewKeyboard);
    if (m_badUsbAlertCompositeCheck) m_badUsbAlertCompositeCheck->setChecked(settings.badUsbAlertCompositeStorage);
    if (m_badUsbAlertInterfaceDriftCheck) m_badUsbAlertInterfaceDriftCheck->setChecked(settings.badUsbAlertInterfaceDrift);
    if (m_badUsbAlertRapidReconnectCheck) m_badUsbAlertRapidReconnectCheck->setChecked(settings.badUsbAlertRapidReconnect);
    if (m_badUsbAutoBaselineCheck) m_badUsbAutoBaselineCheck->setChecked(settings.badUsbAutoBaselineTrusted);
    if (m_badUsbConfirmCheck) m_badUsbConfirmCheck->setChecked(settings.badUsbConfirmAnomalies);
    if (m_badUsbUsbmonCheck) m_badUsbUsbmonCheck->setChecked(settings.badUsbUsbmonEnabled);
    if (m_badUsbUsbmonOnAnomalyCheck) m_badUsbUsbmonOnAnomalyCheck->setChecked(settings.badUsbUsbmonOnAnomalyOnly);
    if (m_badUsbUsbmonCommandEdit) m_badUsbUsbmonCommandEdit->setText(settings.badUsbUsbmonCommand);
    m_defaultTrustCombo->setCurrentIndex(settings.defaultTrustLevel);
    
    // Hashing
    int algoIndex = m_hashAlgorithmCombo->findText(settings.hashAlgorithm);
    if (algoIndex >= 0) {
        m_hashAlgorithmCombo->setCurrentIndex(algoIndex);
    }
    m_bufferSizeSpin->setValue(settings.hashBufferSizeKB);
    m_useMemoryMappingCheck->setChecked(settings.useMemoryMapping);
    m_maxConcurrentSpin->setValue(settings.maxConcurrentHashes);
    
    // Appearance
    int themeIndex = m_themeList.indexOf(FSStyle.currentTheme());
    if (themeIndex >= 0) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }
    m_animationsCheck->setChecked(settings.animationsEnabled);
    
    // Database
    m_databasePathEdit->setText(settings.databasePath);
    
    m_hasChanges = false;
    m_blockSignals = false;
}

AppSettings SettingsDialog::getSettings() const
{
    AppSettings settings;
    
    // General
    settings.startMinimized = m_startMinimizedCheck->isChecked();
    settings.autoStartAtLogin = m_autoStartCheck->isChecked();
    settings.minimizeToTray = m_minimizeToTrayCheck->isChecked();
    settings.showNotifications = m_showNotificationsCheck->isChecked();
    
    // Security
    settings.autoHashOnConnect = m_autoHashOnConnectCheck->isChecked();
    settings.autoHashOnEject = m_autoHashOnEjectCheck->isChecked();
    settings.requireConfirmationForNew = m_confirmNewDeviceCheck->isChecked();
    settings.requireConfirmationForModified = m_confirmModifiedCheck->isChecked();
    settings.promptPerPartition = m_promptPerPartitionCheck->isChecked();
    settings.blockModifiedDevices = m_blockModifiedCheck->isChecked();
    settings.appModule = static_cast<AppModule>(m_appModuleCombo->currentData().toInt());
    settings.defaultVerificationProfile = static_cast<VerificationProfile>(
        m_defaultProfileCombo->currentData().toInt());
    settings.isoScanDirectory = m_isoDirEdit->text().trimmed();
    settings.isoAutoVerifyOnScan = m_isoAutoVerifyCheck->isChecked();
    if (m_isoAutoVerifyOnUsbMountCheck)
        settings.isoAutoVerifyOnUsbMount = m_isoAutoVerifyOnUsbMountCheck->isChecked();
    if (m_blockMountOnIsoFailCheck)
        settings.blockMountOnIsoVerifyFailure = m_blockMountOnIsoFailCheck->isChecked();
    if (m_isoVerifyDecompressedCheck)
        settings.isoVerifyDecompressed = m_isoVerifyDecompressedCheck->isChecked();
    if (m_isoPreferOfflineCheck)
        settings.isoPreferOfflineSidecars = m_isoPreferOfflineCheck->isChecked();
    if (m_isoParallelSpin)
        settings.isoVerifyParallel = m_isoParallelSpin->value();
    if (m_settingsProfileCombo) {
        settings.settingsProfile =
            SettingsProfiles::normalizeProfileId(m_settingsProfileCombo->currentData().toString());
    }
    if (m_badUsbEnabledCheck) settings.badUsbEnabled = m_badUsbEnabledCheck->isChecked();
    if (m_badUsbAlertNewKeyboardCheck) settings.badUsbAlertNewKeyboard = m_badUsbAlertNewKeyboardCheck->isChecked();
    if (m_badUsbAlertCompositeCheck) settings.badUsbAlertCompositeStorage = m_badUsbAlertCompositeCheck->isChecked();
    if (m_badUsbAlertInterfaceDriftCheck) settings.badUsbAlertInterfaceDrift = m_badUsbAlertInterfaceDriftCheck->isChecked();
    if (m_badUsbAlertRapidReconnectCheck) settings.badUsbAlertRapidReconnect = m_badUsbAlertRapidReconnectCheck->isChecked();
    if (m_badUsbAutoBaselineCheck) settings.badUsbAutoBaselineTrusted = m_badUsbAutoBaselineCheck->isChecked();
    if (m_badUsbConfirmCheck) settings.badUsbConfirmAnomalies = m_badUsbConfirmCheck->isChecked();
    if (m_badUsbUsbmonCheck) settings.badUsbUsbmonEnabled = m_badUsbUsbmonCheck->isChecked();
    if (m_badUsbUsbmonOnAnomalyCheck) settings.badUsbUsbmonOnAnomalyOnly = m_badUsbUsbmonOnAnomalyCheck->isChecked();
    if (m_badUsbUsbmonCommandEdit) settings.badUsbUsbmonCommand = m_badUsbUsbmonCommandEdit->text().trimmed();
    settings.defaultTrustLevel = m_defaultTrustCombo->currentIndex();
    
    // Hashing
    settings.hashAlgorithm = m_hashAlgorithmCombo->currentText();
    settings.hashBufferSizeKB = m_bufferSizeSpin->value();
    settings.useMemoryMapping = m_useMemoryMappingCheck->isChecked();
    settings.maxConcurrentHashes = m_maxConcurrentSpin->value();
    
    // Appearance
    settings.theme = m_themeCombo->currentText();
    settings.animationsEnabled = m_animationsCheck->isChecked();
    
    // Database
    settings.databasePath = m_databasePathEdit->text();
    
    return settings;
}

void SettingsDialog::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(16);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // Tab widget
    m_tabWidget = new QTabWidget;
    m_tabWidget->addTab(createGeneralTab(), "General");
    m_tabWidget->addTab(createSecurityTab(), "Security");
    m_tabWidget->addTab(createVerificationTab(), "Verification");
    m_tabWidget->addTab(createHashingTab(), "Hashing");
    m_tabWidget->addTab(createAppearanceTab(), "Appearance");
    m_tabWidget->addTab(createDatabaseTab(), "Database");
    m_tabWidget->addTab(createAboutTab(), "About");
    
    m_mainLayout->addWidget(m_tabWidget);
    
    // Button box
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_restoreDefaultsBtn = new QPushButton("Restore Defaults");
    connect(m_restoreDefaultsBtn, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaults);
    buttonLayout->addWidget(m_restoreDefaultsBtn);
    
    buttonLayout->addStretch();
    
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::onRejected);
    buttonLayout->addWidget(m_buttonBox);
    
    m_mainLayout->addLayout(buttonLayout);
}

QWidget* SettingsDialog::createGeneralTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setSpacing(16);
    
    // Startup group
    QGroupBox* startupGroup = new QGroupBox("Startup");
    QVBoxLayout* startupLayout = new QVBoxLayout(startupGroup);
    
    m_startMinimizedCheck = new QCheckBox("Start minimized to tray");
    m_startMinimizedCheck->setToolTip("Start FlashSentry minimized to the system tray");
    connect(m_startMinimizedCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    startupLayout->addWidget(m_startMinimizedCheck);
    
    m_autoStartCheck = new QCheckBox("Start automatically at login");
    const QString autostartBackend = AutostartManager::backendDescription();
    if (AutostartManager::isAvailable()) {
        m_autoStartCheck->setToolTip(
            autostartBackend.isEmpty()
                ? QStringLiteral("Launch FlashSentry when you log in (minimized to tray)")
                : QStringLiteral("Launch FlashSentry when you log in via %1").arg(autostartBackend));
        connect(m_autoStartCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    } else {
        m_autoStartCheck->setToolTip(
            QStringLiteral("Install the Arch package (systemd user service) or use a desktop session with XDG autostart support"));
        m_autoStartCheck->setEnabled(false);
    }
    startupLayout->addWidget(m_autoStartCheck);
    
    layout->addWidget(startupGroup);
    
    // Behavior group
    QGroupBox* behaviorGroup = new QGroupBox("Behavior");
    QVBoxLayout* behaviorLayout = new QVBoxLayout(behaviorGroup);
    
    m_minimizeToTrayCheck = new QCheckBox("Minimize to system tray instead of closing");
    m_minimizeToTrayCheck->setToolTip("Keep FlashSentry running in the background when you close the window");
    connect(m_minimizeToTrayCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    behaviorLayout->addWidget(m_minimizeToTrayCheck);
    
    m_showNotificationsCheck = new QCheckBox("Show desktop notifications");
    m_showNotificationsCheck->setToolTip("Display notifications for device events");
    connect(m_showNotificationsCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    behaviorLayout->addWidget(m_showNotificationsCheck);
    
    layout->addWidget(behaviorGroup);
    
    layout->addStretch();
    
    return tab;
}


QWidget* SettingsDialog::createVerificationTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);

    auto* intro = new QLabel(QStringLiteral(
        "Recommended for most users: <b>ISO verification</b> (no PGP/Kleopatra knowledge needed) "
        "or <b>watch lists</b> that hash only the folders you care about. "
        "Full raw-partition hashing is available but slow — enable it under Security if you need it."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QGroupBox* presetGroup = new QGroupBox(QStringLiteral("Quick profile"));
    QFormLayout* presetForm = new QFormLayout(presetGroup);
    m_settingsProfileCombo = new QComboBox;
    for (const QString& id : SettingsProfiles::profileIds()) {
        m_settingsProfileCombo->addItem(SettingsProfiles::profileDisplayName(id), id);
    }
    connect(m_settingsProfileCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_blockSignals || index < 0) {
            return;
        }
        AppSettings s = getSettings();
        SettingsProfiles::applyProfile(
            SettingsProfiles::normalizeProfileId(m_settingsProfileCombo->itemData(index).toString()), s);
        loadSettings(s);
        m_hasChanges = true;
    });
    presetForm->addRow(QStringLiteral("Preset:"), m_settingsProfileCombo);
    layout->addWidget(presetGroup);

    QGroupBox* moduleGroup = new QGroupBox(QStringLiteral("Application mode"));
    QFormLayout* moduleForm = new QFormLayout(moduleGroup);
    m_appModuleCombo = new QComboBox;
    m_appModuleCombo->addItem(QStringLiteral("USB drive monitor"), static_cast<int>(AppModule::UsbMonitor));
    m_appModuleCombo->addItem(QStringLiteral("Automatic ISO verification"), static_cast<int>(AppModule::IsoVerifier));
    m_appModuleCombo->addItem(QStringLiteral("BadUSB behavior monitor"), static_cast<int>(AppModule::BadUsbMonitor));
    connect(m_appModuleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onSettingChanged);
    moduleForm->addRow(QStringLiteral("Mode:"), m_appModuleCombo);
    layout->addWidget(moduleGroup);

    QGroupBox* profileGroup = new QGroupBox(QStringLiteral("USB file-tree verification (default)"));
    QFormLayout* profileForm = new QFormLayout(profileGroup);
    m_defaultProfileCombo = new QComboBox;
    m_defaultProfileCombo->addItem(QStringLiteral("Watch folders only (Merkle, recommended)"),
                                   static_cast<int>(VerificationProfile::WatchManifest));
    m_defaultProfileCombo->addItem(QStringLiteral("Full partition (raw hash, advanced)"),
                                   static_cast<int>(VerificationProfile::FullPartition));
    m_defaultProfileCombo->addItem(QStringLiteral("Hybrid (watch + full hash)"),
                                   static_cast<int>(VerificationProfile::Hybrid));
    connect(m_defaultProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onSettingChanged);
    profileForm->addRow(QStringLiteral("Default:"), m_defaultProfileCombo);
    layout->addWidget(profileGroup);

    QGroupBox* isoGroup = new QGroupBox(QStringLiteral("ISO verification module"));
    QFormLayout* isoForm = new QFormLayout(isoGroup);
    m_isoDirEdit = new QLineEdit;
    m_isoDirEdit->setPlaceholderText(QStringLiteral("~/Downloads"));
    connect(m_isoDirEdit, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    isoForm->addRow(QStringLiteral("Scan folder:"), m_isoDirEdit);
    m_isoAutoVerifyCheck = new QCheckBox(QStringLiteral("Verify ISOs automatically after scan"));
    m_isoAutoVerifyOnUsbMountCheck = new QCheckBox(
        QStringLiteral("Automatically verify images when a USB drive is mounted"));
    m_isoAutoVerifyOnUsbMountCheck->setToolTip(
        QStringLiteral("Works with desktop automount: verifies the existing mount without remounting. "
                       "Skips boot-loader and multiboot config folders (EFI, vendor trees)."));
    m_isoAutoVerifyOnUsbMountCheck->setChecked(true);
    connect(m_isoAutoVerifyOnUsbMountCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    connect(m_isoAutoVerifyCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    isoForm->addRow(QStringLiteral(""), m_isoAutoVerifyCheck);
    isoForm->addRow(QStringLiteral(""), m_isoAutoVerifyOnUsbMountCheck);
    m_blockMountOnIsoFailCheck = new QCheckBox(QStringLiteral("Block mount when any image on the stick fails verification"));
    connect(m_blockMountOnIsoFailCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    isoForm->addRow(QStringLiteral(""), m_blockMountOnIsoFailCheck);
    m_isoParallelSpin = new QSpinBox;
    m_isoParallelSpin->setRange(1, 8);
    m_isoParallelSpin->setValue(2);
    connect(m_isoParallelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::onSettingChanged);
    isoForm->addRow(QStringLiteral("Parallel verify:"), m_isoParallelSpin);
    m_isoVerifyDecompressedCheck = new QCheckBox(QStringLiteral("Verify decompressed .img.xz payload (requires xz)"));
    connect(m_isoVerifyDecompressedCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    isoForm->addRow(QStringLiteral(""), m_isoVerifyDecompressedCheck);
    m_isoPreferOfflineCheck = new QCheckBox(QStringLiteral("Prefer local .sha256 sidecars before downloading checksums"));
    connect(m_isoPreferOfflineCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    isoForm->addRow(QStringLiteral(""), m_isoPreferOfflineCheck);
    layout->addWidget(isoGroup);

    QGroupBox* badUsbGroup = new QGroupBox(QStringLiteral("BadUSB behavior monitoring"));
    QFormLayout* badUsbForm = new QFormLayout(badUsbGroup);
    m_badUsbEnabledCheck = new QCheckBox(QStringLiteral("Monitor USB HID keyboards, mice, and controllers"));
    connect(m_badUsbEnabledCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    badUsbForm->addRow(QStringLiteral(""), m_badUsbEnabledCheck);
    m_badUsbAlertNewKeyboardCheck = new QCheckBox(QStringLiteral("Alert on new untrusted keyboards"));
    m_badUsbAlertCompositeCheck = new QCheckBox(QStringLiteral("Alert on keyboard + storage composites"));
    m_badUsbAlertInterfaceDriftCheck = new QCheckBox(QStringLiteral("Alert when a HID interface changes from baseline"));
    m_badUsbAlertRapidReconnectCheck = new QCheckBox(QStringLiteral("Alert on rapid HID reconnect behavior"));
    m_badUsbAutoBaselineCheck = new QCheckBox(QStringLiteral("Automatically trust non-anomalous HID devices"));
    m_badUsbConfirmCheck = new QCheckBox(QStringLiteral("Show an alert dialog for anomalies"));
    for (QCheckBox* box : {m_badUsbAlertNewKeyboardCheck, m_badUsbAlertCompositeCheck,
                           m_badUsbAlertInterfaceDriftCheck, m_badUsbAlertRapidReconnectCheck,
                           m_badUsbAutoBaselineCheck, m_badUsbConfirmCheck}) {
        connect(box, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
        badUsbForm->addRow(QStringLiteral(""), box);
    }
    m_badUsbUsbmonCheck = new QCheckBox(QStringLiteral("Start usbmon/tcpdump capture on anomalies"));
    m_badUsbUsbmonOnAnomalyCheck = new QCheckBox(QStringLiteral("Capture only when an anomaly is detected"));
    connect(m_badUsbUsbmonCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    connect(m_badUsbUsbmonOnAnomalyCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    badUsbForm->addRow(QStringLiteral(""), m_badUsbUsbmonCheck);
    badUsbForm->addRow(QStringLiteral(""), m_badUsbUsbmonOnAnomalyCheck);
    m_badUsbUsbmonCommandEdit = new QLineEdit;
    m_badUsbUsbmonCommandEdit->setPlaceholderText(QStringLiteral("tcpdump -i usbmon{bus} -w {out} -G 30 -W 1"));
    m_badUsbUsbmonCommandEdit->setToolTip(QStringLiteral("Template variables: {bus}, {out}, {stable_id}, {rule_id}"));
    connect(m_badUsbUsbmonCommandEdit, &QLineEdit::textChanged, this, &SettingsDialog::onSettingChanged);
    badUsbForm->addRow(QStringLiteral("Capture command:"), m_badUsbUsbmonCommandEdit);
    layout->addWidget(badUsbGroup);

    layout->addStretch();
    return tab;
}

QWidget* SettingsDialog::createSecurityTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setSpacing(16);
    
    // Auto-hashing group
    QGroupBox* hashingGroup = new QGroupBox("Full-drive hashing (advanced)");
    QVBoxLayout* hashingLayout = new QVBoxLayout(hashingGroup);
    
    m_autoHashOnConnectCheck = new QCheckBox("Hash entire partition when a device is connected");
    m_autoHashOnConnectCheck->setToolTip("Slow: reads the whole block device. Prefer watch folders or ISO mode unless you need a full-disk fingerprint.");
    connect(m_autoHashOnConnectCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    hashingLayout->addWidget(m_autoHashOnConnectCheck);
    
    m_autoHashOnEjectCheck = new QCheckBox("Re-hash devices before ejecting");
    m_autoHashOnEjectCheck->setToolTip("Recalculate hash before safely ejecting a device");
    connect(m_autoHashOnEjectCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    hashingLayout->addWidget(m_autoHashOnEjectCheck);
    
    layout->addWidget(hashingGroup);
    
    // Confirmation group
    QGroupBox* confirmGroup = new QGroupBox("Confirmations");
    QVBoxLayout* confirmLayout = new QVBoxLayout(confirmGroup);
    
    m_confirmNewDeviceCheck = new QCheckBox("Ask before mounting new/unknown devices");
    m_confirmNewDeviceCheck->setToolTip("Prompt for confirmation when an unrecognized device is connected");
    connect(m_confirmNewDeviceCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    confirmLayout->addWidget(m_confirmNewDeviceCheck);
    
    m_confirmModifiedCheck = new QCheckBox("Show tamper alert when hash does not match");
    m_confirmModifiedCheck->setToolTip(
        "When you manually mount a modified partition, show expected vs actual hash "
        "and require confirmation (uses the verification hash; no re-hash)");
    connect(m_confirmModifiedCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    confirmLayout->addWidget(m_confirmModifiedCheck);

    m_promptPerPartitionCheck = new QCheckBox("Prompt separately for each partition");
    m_promptPerPartitionCheck->setToolTip(
        "Default: one whitelist prompt per physical drive (all partitions). "
        "Enable to prompt once per partition instead.");
    connect(m_promptPerPartitionCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    confirmLayout->addWidget(m_promptPerPartitionCheck);
    
    m_blockModifiedCheck = new QCheckBox("Block mounting of modified devices");
    m_blockModifiedCheck->setToolTip(
        "Prevent automatic mounting when verification fails. You can still use "
        "\"Accept fingerprint\" on the device card to update the whitelist.");
    connect(m_blockModifiedCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    confirmLayout->addWidget(m_blockModifiedCheck);
    
    layout->addWidget(confirmGroup);
    
    // Trust group
    QGroupBox* trustGroup = new QGroupBox("Trust Settings");
    QFormLayout* trustLayout = new QFormLayout(trustGroup);
    
    m_defaultTrustCombo = new QComboBox;
    m_defaultTrustCombo->addItems({"New (requires confirmation)", "Trusted", "Always allow"});
    m_defaultTrustCombo->setToolTip("Default trust level for newly whitelisted devices");
    connect(m_defaultTrustCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &SettingsDialog::onSettingChanged);
    trustLayout->addRow("Default trust level:", m_defaultTrustCombo);
    
    layout->addWidget(trustGroup);
    
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createHashingTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setSpacing(16);
    
    // Algorithm group
    QGroupBox* algoGroup = new QGroupBox("Hash Algorithm");
    QFormLayout* algoLayout = new QFormLayout(algoGroup);
    
    m_hashAlgorithmCombo = new QComboBox;
    m_hashAlgorithmCombo->addItems({"SHA256", "SHA512", "BLAKE2b"});
    m_hashAlgorithmCombo->setToolTip("Cryptographic hash algorithm to use for device verification");
    connect(m_hashAlgorithmCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onSettingChanged);
    algoLayout->addRow("Algorithm:", m_hashAlgorithmCombo);
    
    QLabel* algoNote = new QLabel(
        "<small>SHA256 is recommended for most users. "
        "SHA512 and BLAKE2b provide stronger security but may be slower.</small>");
    algoNote->setWordWrap(true);
    algoNote->setStyleSheet(QString("color: %1;").arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    algoLayout->addRow(algoNote);
    
    layout->addWidget(algoGroup);
    
    // Performance group
    QGroupBox* perfGroup = new QGroupBox("Performance");
    QFormLayout* perfLayout = new QFormLayout(perfGroup);
    
    QHBoxLayout* bufferLayout = new QHBoxLayout;
    m_bufferSizeSpin = new QSpinBox;
    m_bufferSizeSpin->setRange(64, 16384);  // 64 KB to 16 MB
    m_bufferSizeSpin->setSingleStep(256);
    m_bufferSizeSpin->setSuffix(" KB");
    m_bufferSizeSpin->setToolTip("Size of read buffer for hashing. Larger values may improve speed on fast drives.");
    connect(m_bufferSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsDialog::onSettingChanged);
    bufferLayout->addWidget(m_bufferSizeSpin);
    
    m_bufferSizeLabel = new QLabel;
    m_bufferSizeLabel->setStyleSheet(QString("color: %1;").arg(FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    bufferLayout->addWidget(m_bufferSizeLabel);
    bufferLayout->addStretch();
    
    perfLayout->addRow("Buffer size:", bufferLayout);
    
    m_useMemoryMappingCheck = new QCheckBox("Use memory-mapped I/O");
    m_useMemoryMappingCheck->setToolTip("Use mmap for faster reading on supported filesystems");
    connect(m_useMemoryMappingCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    perfLayout->addRow(m_useMemoryMappingCheck);
    
    m_maxConcurrentSpin = new QSpinBox;
    m_maxConcurrentSpin->setRange(1, 4);
    m_maxConcurrentSpin->setToolTip("Maximum number of devices to hash simultaneously");
    connect(m_maxConcurrentSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsDialog::onSettingChanged);
    perfLayout->addRow("Max concurrent hashes:", m_maxConcurrentSpin);
    
    layout->addWidget(perfGroup);
    
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createAppearanceTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setSpacing(16);
    
    // Theme group
    QGroupBox* themeGroup = new QGroupBox("Theme");
    QVBoxLayout* themeLayout = new QVBoxLayout(themeGroup);
    
    QHBoxLayout* themeSelectLayout = new QHBoxLayout;
    QLabel* themeLabel = new QLabel("Color theme:");
    themeSelectLayout->addWidget(themeLabel);
    
    m_themeCombo = new QComboBox;
    for (const auto& theme : m_themeList) {
        m_themeCombo->addItem(FSStyle.themeName(theme));
    }
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onThemeChanged);
    themeSelectLayout->addWidget(m_themeCombo);
    themeSelectLayout->addStretch();
    
    themeLayout->addLayout(themeSelectLayout);
    
    // Theme preview
    m_themePreviewLabel = new QLabel;
    m_themePreviewLabel->setMinimumHeight(60);
    m_themePreviewLabel->setAlignment(Qt::AlignCenter);
    m_themePreviewLabel->setText("Theme Preview");
    themeLayout->addWidget(m_themePreviewLabel);
    
    layout->addWidget(themeGroup);
    
    // Effects group
    QGroupBox* effectsGroup = new QGroupBox("Effects");
    QVBoxLayout* effectsLayout = new QVBoxLayout(effectsGroup);
    
    m_animationsCheck = new QCheckBox("Enable animations");
    m_animationsCheck->setToolTip("Enable smooth animations and transitions");
    connect(m_animationsCheck, &QCheckBox::toggled, this, &SettingsDialog::onSettingChanged);
    effectsLayout->addWidget(m_animationsCheck);
    
    QHBoxLayout* fontLayout = new QHBoxLayout;
    QLabel* fontLabel = new QLabel("Font size:");
    fontLayout->addWidget(fontLabel);
    
    m_fontSizeSlider = new QSlider(Qt::Horizontal);
    m_fontSizeSlider->setRange(8, 16);
    m_fontSizeSlider->setTickPosition(QSlider::TicksBelow);
    m_fontSizeSlider->setTickInterval(1);
    connect(m_fontSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_fontSizeLabel->setText(QString("%1 pt").arg(value));
        onSettingChanged();
    });
    fontLayout->addWidget(m_fontSizeSlider, 1);
    
    m_fontSizeLabel = new QLabel("10 pt");
    m_fontSizeLabel->setMinimumWidth(40);
    fontLayout->addWidget(m_fontSizeLabel);
    
    effectsLayout->addLayout(fontLayout);
    
    layout->addWidget(effectsGroup);
    
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createDatabaseTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setSpacing(16);
    
    // Location group
    QGroupBox* locationGroup = new QGroupBox("Database Location");
    QVBoxLayout* locationLayout = new QVBoxLayout(locationGroup);
    
    QHBoxLayout* pathLayout = new QHBoxLayout;
    m_databasePathEdit = new QLineEdit;
    m_databasePathEdit->setPlaceholderText("Default location");
    m_databasePathEdit->setReadOnly(true);
    pathLayout->addWidget(m_databasePathEdit);
    
    m_browseDatabaseBtn = new QPushButton("Browse...");
    connect(m_browseDatabaseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseDatabasePath);
    pathLayout->addWidget(m_browseDatabaseBtn);
    
    locationLayout->addLayout(pathLayout);
    
    m_databaseStatsLabel = new QLabel("Database statistics will appear here");
    m_databaseStatsLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    locationLayout->addWidget(m_databaseStatsLabel);
    
    layout->addWidget(locationGroup);
    
    // Actions group
    QGroupBox* actionsGroup = new QGroupBox("Database Actions");
    QGridLayout* actionsLayout = new QGridLayout(actionsGroup);
    actionsLayout->setSpacing(12);
    
    m_exportBtn = new QPushButton("📤 Export Database");
    m_exportBtn->setToolTip("Export the device whitelist to a file");
    connect(m_exportBtn, &QPushButton::clicked, this, &SettingsDialog::onExportDatabase);
    actionsLayout->addWidget(m_exportBtn, 0, 0);
    
    m_importBtn = new QPushButton("📥 Import Database");
    m_importBtn->setToolTip("Import devices from a file");
    connect(m_importBtn, &QPushButton::clicked, this, &SettingsDialog::onImportDatabase);
    actionsLayout->addWidget(m_importBtn, 0, 1);
    
    m_backupBtn = new QPushButton("💾 Create Backup");
    m_backupBtn->setToolTip("Create a backup of the current database");
    connect(m_backupBtn, &QPushButton::clicked, this, &SettingsDialog::onBackupDatabase);
    actionsLayout->addWidget(m_backupBtn, 1, 0);
    
    m_clearBtn = new QPushButton("🗑️ Clear Database");
    m_clearBtn->setToolTip("Remove all devices from the whitelist");
    m_clearBtn->setStyleSheet(FSStyle.dangerButtonStyleSheet());
    connect(m_clearBtn, &QPushButton::clicked, this, &SettingsDialog::onClearDatabase);
    actionsLayout->addWidget(m_clearBtn, 1, 1);
    
    layout->addWidget(actionsGroup);
    
    layout->addStretch();
    
    return tab;
}

QWidget* SettingsDialog::createAboutTab()
{
    QWidget* tab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(tab);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);
    
    // App icon/logo
    QLabel* logoLabel = new QLabel;
    logoLabel->setText("🛡️");
    logoLabel->setStyleSheet("font-size: 64px;");
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);
    
    // App name
    QLabel* nameLabel = new QLabel("FlashSentry");
    nameLabel->setFont(FSFont(Heading1));
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    layout->addWidget(nameLabel);
    
    // Version
    QLabel* versionLabel = new QLabel(
        QStringLiteral("Version %1").arg(QApplication::applicationVersion()));
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(versionLabel);
    
    // Description
    QLabel* descLabel = new QLabel(
        "USB Flash Drive Security Monitor\n\n"
        "Protect your system by tracking and verifying\n"
        "USB storage devices through cryptographic hashing.");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(descLabel);
    
    layout->addSpacing(20);
    
    // Links
    QHBoxLayout* linksLayout = new QHBoxLayout;
    linksLayout->setAlignment(Qt::AlignCenter);
    linksLayout->setSpacing(20);
    
    QPushButton* githubBtn = new QPushButton("GitHub");
    githubBtn->setCursor(Qt::PointingHandCursor);
    connect(githubBtn, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/flashsentry"));
    });
    linksLayout->addWidget(githubBtn);
    
    QPushButton* licenseBtn = new QPushButton("License");
    licenseBtn->setCursor(Qt::PointingHandCursor);
    connect(licenseBtn, &QPushButton::clicked, [this]() {
        QMessageBox::information(this, "License",
            "FlashSentry is licensed under the MIT License.\n\n"
            "Copyright (c) 2024 FlashSentry Contributors\n\n"
            "Permission is hereby granted, free of charge, to any person "
            "obtaining a copy of this software...");
    });
    linksLayout->addWidget(licenseBtn);
    
    layout->addLayout(linksLayout);
    
    // System info
    layout->addSpacing(20);
    
    QLabel* sysInfoLabel = new QLabel(QString(
        "Qt %1 | Built with C++20\n"
        "Running on %2")
        .arg(qVersion())
        .arg(QSysInfo::prettyProductName()));
    sysInfoLabel->setAlignment(Qt::AlignCenter);
    sysInfoLabel->setStyleSheet(QString("color: %1; font-size: 9pt;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    layout->addWidget(sysInfoLabel);
    
    return tab;
}

void SettingsDialog::applyStyle()
{
    setStyleSheet(FSStyle.dialogStyleSheet() + FSStyle.tabWidgetStyleSheet());
}

void SettingsDialog::onThemeChanged(int index)
{
    if (m_blockSignals || index < 0 || index >= m_themeList.size()) return;
    
    StyleManager::Theme theme = m_themeList[index];
    emit themeChanged(theme);
    
    // Update preview
    QColor bg = FSStyle.color(StyleManager::ColorRole::Surface);
    QColor accent = FSStyle.color(StyleManager::ColorRole::AccentPrimary);
    QColor text = FSStyle.color(StyleManager::ColorRole::TextPrimary);
    
    m_themePreviewLabel->setStyleSheet(QString(
        "background-color: %1; color: %2; border: 2px solid %3; border-radius: 8px; padding: 16px;")
        .arg(bg.name()).arg(text.name()).arg(accent.name()));
    
    onSettingChanged();
}

void SettingsDialog::onSettingChanged()
{
    if (m_blockSignals) return;
    m_hasChanges = true;
}

void SettingsDialog::onExportDatabase()
{
    QString path = QFileDialog::getSaveFileName(
        this,
        "Export Database",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/flashsentry_export.json",
        "JSON Files (*.json);;All Files (*)");
    
    if (!path.isEmpty()) {
        emit exportDatabaseRequested(path);
    }
}

void SettingsDialog::onImportDatabase()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        "Import Database",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON Files (*.json);;All Files (*)");
    
    if (!path.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Import Database",
            "Do you want to merge with existing devices or replace all?\n\n"
            "Click 'Yes' to merge, 'No' to replace.",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        
        if (reply != QMessageBox::Cancel) {
            emit importDatabaseRequested(path, reply == QMessageBox::Yes);
        }
    }
}

void SettingsDialog::onBackupDatabase()
{
    emit backupDatabaseRequested();
    QMessageBox::information(this, "Backup Created", "Database backup has been created successfully.");
}

void SettingsDialog::onClearDatabase()
{
    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        "Clear Database",
        "Are you sure you want to remove ALL devices from the whitelist?\n\n"
        "This action cannot be undone!",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        emit clearDatabaseRequested();
    }
}

void SettingsDialog::onBrowseDatabasePath()
{
    QString path = QFileDialog::getSaveFileName(
        this,
        "Select Database Location",
        m_databasePathEdit->text().isEmpty() ? 
            QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) :
            m_databasePathEdit->text(),
        "JSON Files (*.json);;All Files (*)");
    
    if (!path.isEmpty()) {
        m_databasePathEdit->setText(path);
        onSettingChanged();
    }
}

void SettingsDialog::onRestoreDefaults()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Restore Defaults",
        "Are you sure you want to restore all settings to their default values?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        loadSettings(defaultSettings());
        m_hasChanges = true;
    }
}

void SettingsDialog::onAccepted()
{
    accept();
}

void SettingsDialog::onRejected()
{
    if (m_hasChanges) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Unsaved Changes",
            "You have unsaved changes. Are you sure you want to discard them?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            return;
        }
    }
    
    reject();
}

AppSettings SettingsDialog::defaultSettings() const
{
    return AppSettings();
}

} // namespace FlashSentry