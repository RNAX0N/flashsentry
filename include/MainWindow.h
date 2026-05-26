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
#include <QTableWidget>
#include <QTimer>
#include <QTabBar>
#include <QPushButton>
#include <QCloseEvent>
#include <QSettings>
#include <memory>
#include <optional>
#include <QHash>
#include <QSet>

#include "Types.h"
#include "DeviceMonitor.h"
#include "HashWorker.h"
#include "DatabaseManager.h"
#include "MountManager.h"
#include "DeviceCard.h"
#include "TrayIcon.h"
#include "SettingsDialog.h"
#include "StyleManager.h"
#include "ManifestWorker.h"
#include "IsoVerifierWidget.h"
#include "BadUsbWidget.h"
#include "BadUsbBaselineStore.h"
#include "HidDeviceMonitor.h"
#include "UsbmonCapture.h"

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

    bool wantsStartMinimized() const { return m_settings.startMinimized; }

public slots:
    /**
     * @brief Show and raise the window
     */
    void showAndRaise();

    /**
     * @brief Toggle window visibility
     */
    void toggleVisibility();

    void showSettingsDialog();


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

    void onManifestStarted(const QString& jobId, const QString& deviceNode);
    void onManifestCompleted(const QString& jobId, const ManifestVerifyResult& result);
    void onManifestBaselineBuilt(const QString& jobId, const QString& deviceId, const WatchManifest& manifest);
    void onManifestFailed(const QString& jobId, const QString& error);
    void onWatchListRequested(const QString& deviceNode);
    void onIsoLogMessage(const QString& message);
    void onHidConnected(const HidDeviceInfo& device);
    void onHidDisconnected(const QString& stableId);
    void onHidChanged(const HidDeviceInfo& device);
    void onBadUsbTrustRequested(const QString& stableId);
    void onBadUsbCaptureRequested(const QString& stableId);

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

    void handleNewDevicePartition(const DeviceInfo& device);

    QString driveKey(const DeviceInfo& device) const;

    bool isDriveKnown(const DeviceInfo& device) const;

    void whitelistDrivePartitions(const DeviceInfo& device);

    /**
     * @brief Handle known device (verify hash)
     */
    void handleKnownDevice(const DeviceInfo& device, const DeviceRecord& record);

    /**
     * @brief Start hashing a device
     */
    void startHashing(const QString& deviceNode, bool skipUnmount = false);

    void hashAllPartitionsOnParent(const DeviceInfo& device);

    QString canonicalDeviceId(const DeviceInfo& device);

    void mountIfVerified(const QString& deviceNode);

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
    void refreshDeviceTable();
    void appendEventRow(const QString& event, const QString& device, const QString& type,
                        const QString& result, const QString& details, LogLevel level);
    void updateNavigationState(int index);

    /**
     * @brief Show the new device dialog
     */
    bool showNewDeviceDialog(const DeviceInfo& device);

    bool showNewDriveDialog(const DeviceInfo& device);

    void acceptFingerprint(const DeviceInfo& device, const QString& actualHash,
                         const QString& algorithm, bool mountAfter = false);

    void acceptFingerprintAndMount(const DeviceInfo& device, const QString& actualHash,
                                   const QString& algorithm);

    void startDeviceVerification(const QString& deviceNode);
    void startManifestVerification(const QString& deviceNode);
    void openWatchListDialog(const QString& deviceNode);
    void applyAppModule();
    void syncModeTabFromSettings();
    void onModeTabChanged(int index);
    void triggerIsoVerificationOnMount(const MountManager::MountResult& result);
    void handleManifestMismatch(const DeviceInfo& device, const ManifestVerifyResult& result);
    void acceptManifestBaseline(const DeviceInfo& device, const WatchManifest& manifest);

    void mountDespiteModification(const DeviceInfo& device);

    void applyIsoVerifyOptions();
    void warnIfCatalogIntegrityFailed();
    void maybeTriggerIsoVerifyForMountedDevice(const DeviceInfo& device);
    void clearIsoVerifyDedupForDevice(const DeviceInfo& device);
    void handleIsoVerificationReport(const QString& deviceNode, const QList<IsoVerifyResult>& results);
    QStringList relatedStorageNodesForHid(const HidDeviceInfo& device) const;
    void processBadUsbDevice(const HidDeviceInfo& device);
    void configureBadUsbMonitoring();
    std::optional<HidDeviceCategory> promptForHidCategory(const HidDeviceInfo& device,
                                                          HidDeviceCategory suggested) const;

    bool showModifiedDeviceAlert(const DeviceInfo& device, const QString& expected,
                                 const QString& actual, bool offerMount = true);

    void onAcceptFingerprintRequested(const QString& deviceNode);

    void offerUnmountWithoutHash(const QString& deviceNode, const QString& error);

    // Backend components
    std::unique_ptr<DeviceMonitor> m_deviceMonitor;
    std::unique_ptr<HashWorker> m_hashWorker;
    std::unique_ptr<ManifestWorker> m_manifestWorker;
    IsoVerifierWidget* m_isoWidget = nullptr;
    BadUsbWidget* m_badUsbWidget = nullptr;
    QStackedWidget* m_appModeStack = nullptr;
    QHash<QString, QString> m_manifestJobDevices;
    QHash<QString, ManifestVerifyResult> m_lastManifestResults;
    bool m_pendingHybridFullHash = false;

    std::unique_ptr<DatabaseManager> m_database;
    std::unique_ptr<MountManager> m_mountManager;
    std::unique_ptr<TrayIcon> m_trayIcon;
    std::unique_ptr<HidDeviceMonitor> m_hidMonitor;
    std::unique_ptr<BadUsbBaselineStore> m_badUsbBaselineStore;
    std::unique_ptr<UsbmonCapture> m_usbmonCapture;

    // Settings
    AppSettings m_settings;
    std::unique_ptr<QSettings> m_qsettings;

    // UI - Main structure
    QWidget* m_centralWidget = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_splitter = nullptr;

    // UI - Header
    QWidget* m_headerWidget = nullptr;
    QLabel* m_titleLabel = nullptr;
    QTabBar* m_modeTabBar = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_settingsBtn = nullptr;
    QList<QPushButton*> m_navButtons;

    // UI - Sidebar
    QWidget* m_sidebarWidget = nullptr;
    QLabel* m_connectedCountLabel = nullptr;
    QLabel* m_whitelistedCountLabel = nullptr;
    QLabel* m_hashingCountLabel = nullptr;
    QLabel* m_blockedCountLabel = nullptr;
    QLabel* m_totalEventsLabel = nullptr;
    QLabel* m_monitoringStatusLabel = nullptr;
    QLabel* m_footerUserLabel = nullptr;
    QLabel* m_footerPolicyLabel = nullptr;
    QLabel* m_footerDatabaseLabel = nullptr;
    QListWidget* m_logList = nullptr;

    // UI - Device list
    QScrollArea* m_deviceScrollArea = nullptr;
    QWidget* m_deviceListWidget = nullptr;
    QVBoxLayout* m_deviceListLayout = nullptr;
    QLabel* m_emptyStateLabel = nullptr;
    QStackedWidget* m_contentStack = nullptr;
    QTableWidget* m_deviceTable = nullptr;
    QTableWidget* m_eventTable = nullptr;
    int m_totalEventCount = 0;

    // UI - Status bar
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_catalogStatusBtn = nullptr;
    QLabel* m_hashStatusLabel = nullptr;

    // Device tracking
    QHash<QString, DeviceCard*> m_deviceCards;  // deviceNode -> card
    QHash<QString, QString> m_hashJobDevices;   // jobId -> deviceNode

    enum class PendingHashAction {
        None,
        MountAfterVerify,
        UnmountAfterVerify,
        RunFullHashAfterManifest
    };

    QHash<QString, PendingHashAction> m_pendingHashActions;
    QHash<QString, QString> m_lastVerificationHashes;
    QHash<QString, QDateTime> m_deviceConnectedAt;
    QSet<QString> m_drivePromptInProgress;
    QSet<QString> m_rejectedDrives;
    QSet<QString> m_unmountBeforeHash;
    QSet<QString> m_isoVerifyTriggeredMounts;
    QHash<QString, QList<QDateTime>> m_hidConnectHistory;

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