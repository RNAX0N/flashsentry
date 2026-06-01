#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>

#include "Types.h"
#include "StyleManager.h"

namespace FlashSpartan {

/**
 * @brief SettingsDialog - Configuration dialog for FlashSpartan
 * 
 * Provides settings for:
 * - General behavior (startup, notifications, tray)
 * - Security options (auto-hash, confirmation dialogs)
 * - Hashing configuration (algorithm, buffer size)
 * - Appearance (theme, animations)
 * - Database management (export, import, backup)
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

    // Prevent copying
    SettingsDialog(const SettingsDialog&) = delete;
    SettingsDialog& operator=(const SettingsDialog&) = delete;

    /**
     * @brief Load current settings into the dialog
     */
    void loadSettings(const AppSettings& settings);

    /**
     * @brief Get the modified settings
     */
    AppSettings getSettings() const;

    /**
     * @brief Check if settings have been modified
     */
    bool hasChanges() const { return m_hasChanges; }

    /** Populate database tab summary (device count, path). */
    void setDatabaseStatistics(int deviceCount, const QString& databasePath);

    /** Embed tabs in the main window (hides OK/Cancel). */
    void setEmbeddedMode(bool embedded);

public slots:
    void restoreDefaultsTriggered();

signals:
    /**
     * @brief Emitted when theme is changed (for live preview)
     */
    void themeChanged(StyleManager::Theme theme);

    /** Emitted when any setting changes (live apply in embedded mode). */
    void liveSettingsChanged(const AppSettings& settings);

    /**
     * @brief Emitted when user requests database export
     */
    void exportDatabaseRequested(const QString& path);

    /**
     * @brief Emitted when user requests database import
     */
    void importDatabaseRequested(const QString& path);

    /**
     * @brief Emitted when user requests database backup
     */
    void backupDatabaseRequested();

    /**
     * @brief Emitted when user requests to clear database
     */
    void clearDatabaseRequested();

private slots:
    void onThemeChanged(int index);
    void onSettingChanged();
    void onExportDatabase();
    void onImportDatabase();
    void onBackupDatabase();
    void onClearDatabase();
    void onBrowseDatabasePath();
    void onRestoreDefaults();
    void onAccepted();
    void onRejected();

private:
    /**
     * @brief Create the dialog UI
     */
    void setupUi();

    /**
     * @brief Create General settings tab
     */
    QWidget* createGeneralTab();

    /**
     * @brief Create Verification / ISO / profiles tab
     */
    QWidget* createVerificationTab();

    /**
     * @brief Create Security settings tab
     */
    QWidget* createSecurityTab();

    /**
     * @brief Create Hashing settings tab
     */
    QWidget* createHashingTab();

    /**
     * @brief Create Appearance settings tab
     */
    QWidget* createAppearanceTab();

    /**
     * @brief Create Database settings tab
     */
    QWidget* createDatabaseTab();

    /**
     * @brief Create About tab
     */
    QWidget* createAboutTab();

    /**
     * @brief Apply styling to the dialog
     */
    void applyStyle();

    /**
     * @brief Get default settings
     */
    AppSettings defaultSettings() const;

    // Main layout
    QVBoxLayout* m_mainLayout = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
    QPushButton* m_restoreDefaultsBtn = nullptr;
    QWidget* m_buttonBar = nullptr;
    bool m_embeddedMode = false;

    // General tab
    QCheckBox* m_startMinimizedCheck = nullptr;
    QCheckBox* m_minimizeToTrayCheck = nullptr;
    QCheckBox* m_showNotificationsCheck = nullptr;
    QCheckBox* m_autoStartCheck = nullptr;
    QSpinBox* m_recentEventsLimitSpin = nullptr;
    QSpinBox* m_deviceHistoryRetentionSpin = nullptr;
    QSpinBox* m_deviceHistoryMaxEntriesSpin = nullptr;
    QComboBox* m_allowedCountModeCombo = nullptr;

    // Security tab
    QCheckBox* m_autoHashOnConnectCheck = nullptr;
    QCheckBox* m_autoHashOnEjectCheck = nullptr;
    QCheckBox* m_confirmNewDeviceCheck = nullptr;
    QCheckBox* m_confirmModifiedCheck = nullptr;
    QCheckBox* m_blockModifiedCheck = nullptr;
    QComboBox* m_defaultTrustCombo = nullptr;
    QComboBox* m_appModuleCombo = nullptr;
    QComboBox* m_defaultProfileCombo = nullptr;
    QLineEdit* m_isoDirEdit = nullptr;
    QCheckBox* m_isoAutoVerifyCheck = nullptr;
    QCheckBox* m_isoAutoVerifyOnUsbMountCheck = nullptr;
    QComboBox* m_settingsProfileCombo = nullptr;
    QLabel* m_profileDescriptionLabel = nullptr;
    QCheckBox* m_blockMountOnIsoFailCheck = nullptr;
    QCheckBox* m_isoVerifyDecompressedCheck = nullptr;
    QCheckBox* m_isoPreferOfflineCheck = nullptr;
    QSpinBox* m_isoParallelSpin = nullptr;
    QCheckBox* m_badUsbEnabledCheck = nullptr;
    QCheckBox* m_badUsbAlertNewKeyboardCheck = nullptr;
    QCheckBox* m_badUsbAlertCompositeCheck = nullptr;
    QCheckBox* m_badUsbAlertInterfaceDriftCheck = nullptr;
    QCheckBox* m_badUsbAlertRapidReconnectCheck = nullptr;
    QCheckBox* m_badUsbAutoBaselineCheck = nullptr;
    QCheckBox* m_badUsbConfirmCheck = nullptr;
    QCheckBox* m_badUsbUsbmonCheck = nullptr;
    QCheckBox* m_badUsbUsbmonOnAnomalyCheck = nullptr;
    QLineEdit* m_badUsbUsbmonCommandEdit = nullptr;

    // Hashing tab
    QComboBox* m_hashAlgorithmCombo = nullptr;
    QComboBox* m_defaultHashScopeCombo = nullptr;
    QComboBox* m_defaultHashScanModeCombo = nullptr;
    QCheckBox* m_hashResumeCheckpointsCheck = nullptr;
    QCheckBox* m_promptHashOptionsCheck = nullptr;
    QSpinBox* m_bufferSizeSpin = nullptr;
    QCheckBox* m_useMemoryMappingCheck = nullptr;
    QSpinBox* m_maxConcurrentSpin = nullptr;
    QLabel* m_bufferSizeLabel = nullptr;

    // Appearance tab
    QComboBox* m_themeCombo = nullptr;
    QCheckBox* m_animationsCheck = nullptr;
    QSlider* m_fontSizeSlider = nullptr;
    QLabel* m_fontSizeLabel = nullptr;
    QLabel* m_themePreviewLabel = nullptr;

    // Database tab
    QLineEdit* m_databasePathEdit = nullptr;
    QPushButton* m_browseDatabaseBtn = nullptr;
    QLabel* m_databaseStatsLabel = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_backupBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    // State
    AppSettings m_originalSettings;
    bool m_hasChanges = false;
    bool m_blockSignals = false;

    // Theme name mapping
    QList<StyleManager::Theme> m_themeList;
};

} // namespace FlashSpartan