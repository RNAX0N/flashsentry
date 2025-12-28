#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QStackedWidget>
#include <QSplitter>
#include <QListWidget>
#include <QTimer>
#include <QCloseEvent>
#include <QSettings>
#include <memory>
#include <QHash>

#include "Types.h"
#include "DeviceMonitor.h"
#include "HashWorker.h"
#include "DatabaseManager.h"
#include "MountManager.h"
#include "DeviceCard.h"
#include "TrayIcon.h"
#include "SettingsDialog.h"
#include "StyleManager.h"

namespace FlashSentry {

/**
 * @brief MainWindow - Main application window for FlashSentry
 * 
 * Integrates all components:
 * - Device monitoring and display
 * - Hash verification
 * - Database management
 * - Mount operations
 * - System tray integration
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Prevent copying
    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /**
     * @brief Check if the application should quit or minimize to tray
     */
    bool shouldMinimizeToTray() const;

public slots:
    /**
     * @brief Show and raise the window
     */
    void showAndRaise();

    /**
     * @brief Toggle window visibility
     */
    void toggleVisibility();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Device events
    void onDeviceConnected(const DeviceInfo& device);
    void onDeviceDisconnected(const QString& deviceNode);
    void onDeviceChanged(const DeviceInfo& device);
    void onInitialScanComplete(int deviceCount);

    // Hash events
    void onHashStarted(const QString& jobId, const QString& deviceNode);
    void onHashProgress(const QString& jobId, double progress, 
                        quint64 bytesProcessed, double speedMBps);
    void onHashCompleted(const QString& jobId, const HashResult& result);
    void onHashFailed(const QString& jobId, const QString& error);
    void onHashCancelled(const QString& jobId);

    // Mount events
    void onMountCompleted(const MountManager::MountResult& result);
    void onUnmountCompleted(const MountManager::UnmountResult& result);
    void onPowerOffCompleted(const QString& deviceNode, bool success, const QString& error);

    // Database events
    void onDatabaseLoaded(int deviceCount);
    void onHashMismatch(const QString& uniqueId, const QString& expected, const QString& actual);

    // Device card actions
    void onMountRequested(const QString& deviceNode);
    void onUnmountRequested(const QString& deviceNode);
    void onEjectRequested(const QString& deviceNode);
    void onRehashRequested(const QString& deviceNode);
    void onOpenMountPointRequested(const QString& mountPoint);
    void onDeviceCardClicked(const QString& deviceNode);

    // Tray events
    void onTrayActivated();
    void onQuitRequested();
    void onSettingsRequested();

    // UI actions
    void onSearchTextChanged(const QString& text);
    void onRefreshClicked();
    void onSettingsClicked();

    // Settings
    void onThemeChanged(StyleManager::Theme theme);
    void applySettings(const AppSettings& settings);

    // Periodic updates
    void updateStatusBar();

private:
    /**
     * @brief Initialize all UI components
     */
    void setupUi();

    /**
     * @brief Create the header/toolbar area
     */
    QWidget* createHeader();

    /**
     * @brief Create the main content area
     */
    QWidget* createMainContent();

    /**
     * @brief Create the sidebar
     */
    QWidget* createSidebar();

    /**
     * @brief Create the device list area
     */
    QWidget* createDeviceListArea();

    /**
     * @brief Create the status bar
     */
    void createStatusBar();

    /**
     * @brief Initialize all backend components
     */
    void initializeBackend();

    /**
     * @brief Connect all signals and slots
     */
    void connectSignals();

    /**
     * @brief Load settings from disk
     */
    void loadSettings();

    /**
     * @brief Save settings to disk
     */
    void saveSettings();

    /**
     * @brief Apply styling to the window
     */
    void applyStyle();

    /**
     * @brief Add a device card to the UI
     */
    DeviceCard* addDeviceCard(const DeviceInfo& device);

    /**
     * @brief Remove a device card from the UI
     */
    void removeDeviceCard(const QString& deviceNode);

    /**
     * @brief Get device card by device node
     */
    DeviceCard* getDeviceCard(const QString& deviceNode);

    /**
     * @brief Update device card for a device
     */
    void updateDeviceCard(const DeviceInfo& device);

    /**
     * @brief Handle new device (prompt user if unknown)
     */
    void handleNewDevice(const DeviceInfo& device);

    /**
     * @brief Handle known device (verify hash)
     */
    void handleKnownDevice(const DeviceInfo& device, const DeviceRecord& record);

    /**
     * @brief Start hashing a device
     */
    void startHashing(const QString& deviceNode);

    /**
     * @brief Log a message to the log panel
     */
    void logMessage(const QString& message, LogLevel level = LogLevel::Info);

    /**
     * @brief Update the empty state message
     */
    void updateEmptyState();

    /**
     * @brief Update sidebar statistics
     */
    void updateSidebarStats();

    /**
     * @brief Show the new device dialog
     */
    bool showNewDeviceDialog(const DeviceInfo& device);

    /**
     * @brief Show the modified device alert
     */
    void showModifiedDeviceAlert(const DeviceInfo& device, const QString& expected, const QString& actual);

    // Backend components
    std::unique_ptr<DeviceMonitor> m_deviceMonitor;
    std::unique_ptr<HashWorker> m_hashWorker;
    std::unique_ptr<DatabaseManager> m_database;
    std::unique_ptr<MountManager> m_mountManager;
    std::unique_ptr<TrayIcon> m_trayIcon;

    // Settings
    AppSettings m_settings;
    std::unique_ptr<QSettings> m_qsettings;

    // UI - Main structure
    QWidget* m_centralWidget = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QSplitter* m_splitter = nullptr;

    // UI - Header
    QWidget* m_headerWidget = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_settingsBtn = nullptr;

    // UI - Sidebar
    QWidget* m_sidebarWidget = nullptr;
    QLabel* m_connectedCountLabel = nullptr;
    QLabel* m_whitelistedCountLabel = nullptr;
    QLabel* m_hashingCountLabel = nullptr;
    QListWidget* m_logList = nullptr;

    // UI - Device list
    QScrollArea* m_deviceScrollArea = nullptr;
    QWidget* m_deviceListWidget = nullptr;
    QVBoxLayout* m_deviceListLayout = nullptr;
    QLabel* m_emptyStateLabel = nullptr;
    QStackedWidget* m_contentStack = nullptr;

    // UI - Status bar
    QLabel* m_statusLabel = nullptr;
    QLabel* m_hashStatusLabel = nullptr;

    // Device tracking
    QHash<QString, DeviceCard*> m_deviceCards;  // deviceNode -> card
    QHash<QString, QString> m_hashJobDevices;   // jobId -> deviceNode

    // State
    bool m_isClosing = false;
    int m_activeHashCount = 0;

    // Timers
    QTimer* m_statusUpdateTimer = nullptr;

    // Constants
    static constexpr int SIDEBAR_WIDTH = 280;
    static constexpr int STATUS_UPDATE_INTERVAL_MS = 1000;
};

} // namespace FlashSentry