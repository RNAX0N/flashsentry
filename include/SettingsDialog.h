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

namespace FlashSentry {

/**
 * @brief SettingsDialog - Configuration dialog for FlashSentry
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

signals:
    /**
     * @brief Emitted when theme is changed (for live preview)
     */
    void themeChanged(StyleManager::Theme theme);

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

    // General tab
    QCheckBox* m_startMinimizedCheck = nullptr;
    QCheckBox* m_minimizeToTrayCheck = nullptr;
    QCheckBox* m_showNotificationsCheck = nullptr;
    QCheckBox* m_autoStartCheck = nullptr;

    // Security tab
    QCheckBox* m_autoHashOnConnectCheck = nullptr;
    QCheckBox* m_autoHashOnEjectCheck = nullptr;
    QCheckBox* m_confirmNewDeviceCheck = nullptr;
    QCheckBox* m_confirmModifiedCheck = nullptr;
    QCheckBox* m_blockModifiedCheck = nullptr;
    QComboBox* m_defaultTrustCombo = nullptr;

    // Hashing tab
    QComboBox* m_hashAlgorithmCombo = nullptr;
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

} // namespace FlashSentry