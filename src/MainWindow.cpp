#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDateTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QScreen>
#include <QStyle>
#include <QGraphicsDropShadowEffect>

namespace FlashSentry {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Initialize settings storage
    m_qsettings = std::make_unique<QSettings>("FlashSentry", "FlashSentry");
    
    // Initialize the style manager
    FSStyle.initialize();
    
    // Setup UI first
    setupUi();
    
    // Initialize backend components
    initializeBackend();
    
    // Connect all signals
    connectSignals();
    
    // Load settings
    loadSettings();
    
    // Apply styling
    applyStyle();
    
    // Setup status update timer
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(STATUS_UPDATE_INTERVAL_MS);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start();
    
    // Start device monitoring
    m_deviceMonitor->startMonitoring();
    
    // Show tray icon
    if (TrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->show();
    }
    
    logMessage("FlashSentry started", LogLevel::Info);
}

MainWindow::~MainWindow()
{
    m_isClosing = true;
    
    // Stop monitoring
    if (m_deviceMonitor) {
        m_deviceMonitor->stopMonitoring();
    }
    
    // Cancel any pending hashes
    if (m_hashWorker) {
        m_hashWorker->cancelAll();
    }
    
    // Save settings
    saveSettings();
    
    // Save database
    if (m_database && m_database->hasUnsavedChanges()) {
        m_database->save();
    }
}

void MainWindow::setupUi()
{
    setWindowTitle("FlashSentry");
    setMinimumSize(900, 600);
    
    // Center on screen
    if (auto* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - 1100) / 2;
        int y = (screenGeometry.height() - 700) / 2;
        setGeometry(x, y, 1100, 700);
    }
    
    // Central widget
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Add header
    m_mainLayout->addWidget(createHeader());
    
    // Add main content
    m_mainLayout->addWidget(createMainContent(), 1);
    
    // Create status bar
    createStatusBar();
    
    // Update empty state
    updateEmptyState();
}

QWidget* MainWindow::createHeader()
{
    m_headerWidget = new QWidget;
    m_headerWidget->setObjectName("HeaderWidget");
    m_headerWidget->setFixedHeight(70);
    
    QHBoxLayout* layout = new QHBoxLayout(m_headerWidget);
    layout->setContentsMargins(20, 12, 20, 12);
    layout->setSpacing(16);
    
    // Logo/Title
    QHBoxLayout* titleLayout = new QHBoxLayout;
    titleLayout->setSpacing(12);
    
    QLabel* logoLabel = new QLabel("🛡️");
    logoLabel->setStyleSheet("font-size: 28px;");
    titleLayout->addWidget(logoLabel);
    
    m_titleLabel = new QLabel("FlashSentry");
    m_titleLabel->setFont(FSFont(Heading2));
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    titleLayout->addWidget(m_titleLabel);
    
    layout->addLayout(titleLayout);
    
    layout->addStretch();
    
    // Search box
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("🔍 Search devices...");
    m_searchEdit->setFixedWidth(250);
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    layout->addWidget(m_searchEdit);
    
    layout->addSpacing(16);
    
    // Refresh button
    m_refreshBtn = new QPushButton("↻ Refresh");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setToolTip("Rescan for USB devices");
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    layout->addWidget(m_refreshBtn);
    
    // Settings button
    m_settingsBtn = new QPushButton("⚙️");
    m_settingsBtn->setFixedSize(40, 40);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("Settings");
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    layout->addWidget(m_settingsBtn);
    
    return m_headerWidget;
}

QWidget* MainWindow::createMainContent()
{
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(1);
    m_splitter->setChildrenCollapsible(false);
    
    // Left side - Device list
    m_splitter->addWidget(createDeviceListArea());
    
    // Right side - Sidebar
    m_splitter->addWidget(createSidebar());
    
    // Set splitter sizes
    m_splitter->setSizes({700, SIDEBAR_WIDTH});
    
    return m_splitter;
}

QWidget* MainWindow::createDeviceListArea()
{
    QWidget* container = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(20, 20, 10, 20);
    layout->setSpacing(16);
    
    // Section header
    QLabel* sectionLabel = new QLabel("Connected Devices");
    sectionLabel->setFont(FSFont(Heading3));
    sectionLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(sectionLabel);
    
    // Stacked widget for content/empty state
    m_contentStack = new QStackedWidget;
    
    // Device scroll area
    m_deviceScrollArea = new QScrollArea;
    m_deviceScrollArea->setWidgetResizable(true);
    m_deviceScrollArea->setFrameShape(QFrame::NoFrame);
    m_deviceScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_deviceListWidget = new QWidget;
    m_deviceListLayout = new QVBoxLayout(m_deviceListWidget);
    m_deviceListLayout->setContentsMargins(0, 0, 0, 0);
    m_deviceListLayout->setSpacing(12);
    m_deviceListLayout->addStretch();
    
    m_deviceScrollArea->setWidget(m_deviceListWidget);
    m_contentStack->addWidget(m_deviceScrollArea);
    
    // Empty state
    QWidget* emptyStateWidget = new QWidget;
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyStateWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    
    m_emptyStateLabel = new QLabel;
    m_emptyStateLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateLabel->setWordWrap(true);
    m_emptyStateLabel->setStyleSheet(QString("color: %1; font-size: 14px;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    emptyLayout->addWidget(m_emptyStateLabel);
    
    m_contentStack->addWidget(emptyStateWidget);
    
    layout->addWidget(m_contentStack, 1);
    
    return container;
}

QWidget* MainWindow::createSidebar()
{
    m_sidebarWidget = new QWidget;
    m_sidebarWidget->setObjectName("SidebarWidget");
    m_sidebarWidget->setMinimumWidth(SIDEBAR_WIDTH);
    m_sidebarWidget->setMaximumWidth(350);
    
    QVBoxLayout* layout = new QVBoxLayout(m_sidebarWidget);
    layout->setContentsMargins(10, 20, 20, 20);
    layout->setSpacing(16);
    
    // Statistics section
    QLabel* statsLabel = new QLabel("Statistics");
    statsLabel->setFont(FSFont(Heading3));
    statsLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(statsLabel);
    
    // Stats grid
    QWidget* statsWidget = new QWidget;
    statsWidget->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            border-radius: 8px;
        }
    )").arg(FSStyle.colorCss(StyleManager::ColorRole::Surface)));
    
    QGridLayout* statsLayout = new QGridLayout(statsWidget);
    statsLayout->setContentsMargins(16, 16, 16, 16);
    statsLayout->setSpacing(12);
    
    auto addStatRow = [&](int row, const QString& label, QLabel*& valueLabel, const QString& icon) {
        QLabel* iconLabel = new QLabel(icon);
        iconLabel->setStyleSheet("font-size: 16px;");
        statsLayout->addWidget(iconLabel, row, 0);
        
        QLabel* textLabel = new QLabel(label);
        textLabel->setStyleSheet(QString("color: %1;").arg(
            FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
        statsLayout->addWidget(textLabel, row, 1);
        
        valueLabel = new QLabel("0");
        valueLabel->setAlignment(Qt::AlignRight);
        valueLabel->setFont(FSFont(Heading3));
        valueLabel->setStyleSheet(QString("color: %1;").arg(
            FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
        statsLayout->addWidget(valueLabel, row, 2);
    };
    
    addStatRow(0, "Connected", m_connectedCountLabel, "📱");
    addStatRow(1, "Whitelisted", m_whitelistedCountLabel, "✅");
    addStatRow(2, "Hashing", m_hashingCountLabel, "⏳");
    
    layout->addWidget(statsWidget);
    
    // Log section
    QLabel* logLabel = new QLabel("Activity Log");
    logLabel->setFont(FSFont(Heading3));
    logLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(logLabel);
    
    m_logList = new QListWidget;
    m_logList->setFrameShape(QFrame::NoFrame);
    m_logList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_logList->setStyleSheet(FSStyle.listWidgetStyleSheet());
    layout->addWidget(m_logList, 1);
    
    return m_sidebarWidget;
}

void MainWindow::createStatusBar()
{
    QStatusBar* status = statusBar();
    status->setStyleSheet(QString(R"(
        QStatusBar {
            background-color: %1;
            border-top: 1px solid %2;
            padding: 4px 12px;
        }
        QStatusBar::item {
            border: none;
        }
    )").arg(FSStyle.colorCss(StyleManager::ColorRole::BackgroundAlt))
       .arg(FSStyle.colorCss(StyleManager::ColorRole::Border)));
    
    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    status->addWidget(m_statusLabel, 1);
    
    m_hashStatusLabel = new QLabel;
    m_hashStatusLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    status->addPermanentWidget(m_hashStatusLabel);
}

void MainWindow::initializeBackend()
{
    // Create device monitor
    m_deviceMonitor = std::make_unique<DeviceMonitor>(this);
    
    // Create hash worker
    m_hashWorker = std::make_unique<HashWorker>(this);
    
    // Create database manager
    m_database = std::make_unique<DatabaseManager>(this);
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) 
                     + "/flashsentry/devices.json";
    m_database->initialize(dbPath);
    
    // Create mount manager
    m_mountManager = std::make_unique<MountManager>(this);
    
    // Create tray icon
    m_trayIcon = std::make_unique<TrayIcon>(this);
}

void MainWindow::connectSignals()
{
    // Device monitor signals
    connect(m_deviceMonitor.get(), &DeviceMonitor::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    connect(m_deviceMonitor.get(), &DeviceMonitor::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);
    connect(m_deviceMonitor.get(), &DeviceMonitor::deviceChanged,
            this, &MainWindow::onDeviceChanged);
    connect(m_deviceMonitor.get(), &DeviceMonitor::initialScanComplete,
            this, &MainWindow::onInitialScanComplete);
    
    // Hash worker signals
    connect(m_hashWorker.get(), &HashWorker::hashStarted,
            this, &MainWindow::onHashStarted);
    connect(m_hashWorker.get(), &HashWorker::hashProgress,
            this, &MainWindow::onHashProgress);
    connect(m_hashWorker.get(), &HashWorker::hashCompleted,
            this, &MainWindow::onHashCompleted);
    connect(m_hashWorker.get(), &HashWorker::hashFailed,
            this, &MainWindow::onHashFailed);
    connect(m_hashWorker.get(), &HashWorker::hashCancelled,
            this, &MainWindow::onHashCancelled);
    
    // Mount manager signals
    connect(m_mountManager.get(), &MountManager::mountCompleted,
            this, &MainWindow::onMountCompleted);
    connect(m_mountManager.get(), &MountManager::unmountCompleted,
            this, &MainWindow::onUnmountCompleted);
    connect(m_mountManager.get(), &MountManager::powerOffCompleted,
            this, &MainWindow::onPowerOffCompleted);
    
    // Database signals
    connect(m_database.get(), &DatabaseManager::databaseLoaded,
            this, &MainWindow::onDatabaseLoaded);
    connect(m_database.get(), &DatabaseManager::hashMismatch,
            this, &MainWindow::onHashMismatch);
    
    // Tray icon signals
    connect(m_trayIcon.get(), &TrayIcon::showWindowRequested,
            this, &MainWindow::showAndRaise);
    connect(m_trayIcon.get(), &TrayIcon::activated,
            this, &MainWindow::onTrayActivated);
    connect(m_trayIcon.get(), &TrayIcon::quitRequested,
            this, &MainWindow::onQuitRequested);
    connect(m_trayIcon.get(), &TrayIcon::settingsRequested,
            this, &MainWindow::onSettingsRequested);
    connect(m_trayIcon.get(), &TrayIcon::deviceEjectRequested,
            this, &MainWindow::onEjectRequested);
}

void MainWindow::loadSettings()
{
    m_settings.startMinimized = m_qsettings->value("general/startMinimized", false).toBool();
    m_settings.minimizeToTray = m_qsettings->value("general/minimizeToTray", true).toBool();
    m_settings.showNotifications = m_qsettings->value("general/showNotifications", true).toBool();
    m_settings.autoHashOnConnect = m_qsettings->value("security/autoHashOnConnect", true).toBool();
    m_settings.autoHashOnEject = m_qsettings->value("security/autoHashOnEject", true).toBool();
    m_settings.requireConfirmationForNew = m_qsettings->value("security/confirmNewDevice", true).toBool();
    m_settings.requireConfirmationForModified = m_qsettings->value("security/confirmModified", true).toBool();
    m_settings.blockModifiedDevices = m_qsettings->value("security/blockModified", false).toBool();
    m_settings.hashAlgorithm = m_qsettings->value("hashing/algorithm", "SHA256").toString();
    m_settings.hashBufferSizeKB = m_qsettings->value("hashing/bufferSizeKB", 1024).toInt();
    m_settings.useMemoryMapping = m_qsettings->value("hashing/useMemoryMapping", true).toBool();
    m_settings.maxConcurrentHashes = m_qsettings->value("hashing/maxConcurrent", 1).toInt();
    m_settings.animationsEnabled = m_qsettings->value("appearance/animations", true).toBool();
    
    QString themeName = m_qsettings->value("appearance/theme", "Cyber Dark").toString();
    for (auto theme : FSStyle.availableThemes()) {
        if (FSStyle.themeName(theme) == themeName) {
            FSStyle.setTheme(theme);
            break;
        }
    }
    
    // Apply settings
    m_trayIcon->setNotificationsEnabled(m_settings.showNotifications);
    m_hashWorker->setMaxConcurrent(m_settings.maxConcurrentHashes);
    FSStyle.setAnimationsEnabled(m_settings.animationsEnabled);
    
    // Restore window geometry
    if (m_qsettings->contains("window/geometry")) {
        restoreGeometry(m_qsettings->value("window/geometry").toByteArray());
    }
}

void MainWindow::saveSettings()
{
    m_qsettings->setValue("general/startMinimized", m_settings.startMinimized);
    m_qsettings->setValue("general/minimizeToTray", m_settings.minimizeToTray);
    m_qsettings->setValue("general/showNotifications", m_settings.showNotifications);
    m_qsettings->setValue("security/autoHashOnConnect", m_settings.autoHashOnConnect);
    m_qsettings->setValue("security/autoHashOnEject", m_settings.autoHashOnEject);
    m_qsettings->setValue("security/confirmNewDevice", m_settings.requireConfirmationForNew);
    m_qsettings->setValue("security/confirmModified", m_settings.requireConfirmationForModified);
    m_qsettings->setValue("security/blockModified", m_settings.blockModifiedDevices);
    m_qsettings->setValue("hashing/algorithm", m_settings.hashAlgorithm);
    m_qsettings->setValue("hashing/bufferSizeKB", m_settings.hashBufferSizeKB);
    m_qsettings->setValue("hashing/useMemoryMapping", m_settings.useMemoryMapping);
    m_qsettings->setValue("hashing/maxConcurrent", m_settings.maxConcurrentHashes);
    m_qsettings->setValue("appearance/animations", m_settings.animationsEnabled);
    m_qsettings->setValue("appearance/theme", FSStyle.themeName(FSStyle.currentTheme()));
    m_qsettings->setValue("window/geometry", saveGeometry());
    
    m_qsettings->sync();
}

void MainWindow::applyStyle()
{
    setStyleSheet(FSStyle.mainWindowStyleSheet());
    
    // Header styling
    m_headerWidget->setStyleSheet(QString(R"(
        QWidget#HeaderWidget {
            background-color: %1;
            border-bottom: 1px solid %2;
        }
    )").arg(FSStyle.colorCss(StyleManager::ColorRole::BackgroundAlt))
       .arg(FSStyle.colorCss(StyleManager::ColorRole::Border)));
    
    // Search field styling
    m_searchEdit->setStyleSheet(FSStyle.inputFieldStyleSheet());
    
    // Button styling
    m_refreshBtn->setStyleSheet(FSStyle.buttonStyleSheet());
    m_settingsBtn->setStyleSheet(FSStyle.buttonStyleSheet());
    
    // Sidebar styling
    m_sidebarWidget->setStyleSheet(QString(R"(
        QWidget#SidebarWidget {
            background-color: %1;
            border-left: 1px solid %2;
        }
    )").arg(FSStyle.colorCss(StyleManager::ColorRole::BackgroundAlt))
       .arg(FSStyle.colorCss(StyleManager::ColorRole::Border)));
    
    // Scroll area styling
    m_deviceScrollArea->setStyleSheet(FSStyle.scrollAreaStyleSheet());
}

bool MainWindow::shouldMinimizeToTray() const
{
    return m_settings.minimizeToTray && TrayIcon::isSystemTrayAvailable();
}

void MainWindow::showAndRaise()
{
    show();
    raise();
    activateWindow();
    m_trayIcon->updateWindowVisibility(true);
}

void MainWindow::toggleVisibility()
{
    if (isVisible()) {
        hide();
    } else {
        showAndRaise();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_isClosing && shouldMinimizeToTray()) {
        hide();
        m_trayIcon->updateWindowVisibility(false);
        event->ignore();
        return;
    }
    
    saveSettings();
    event->accept();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_trayIcon->updateWindowVisibility(true);
}

void MainWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    m_trayIcon->updateWindowVisibility(false);
}

// ============================================================================
// Device Events
// ============================================================================

void MainWindow::onDeviceConnected(const DeviceInfo& device)
{
    logMessage(QString("Device connected: %1 (%2)").arg(device.displayName(), device.deviceNode));
    
    // Add device card
    DeviceCard* card = addDeviceCard(device);
    
    // Check if device is known
    QString deviceId = device.uniqueId();
    
    if (m_database->hasDevice(deviceId)) {
        auto record = m_database->getDevice(deviceId);
        if (record) {
            handleKnownDevice(device, *record);
            m_trayIcon->notifyDeviceConnected(device, true);
        }
    } else {
        handleNewDevice(device);
        m_trayIcon->notifyDeviceConnected(device, false);
    }
    
    updateSidebarStats();
    updateEmptyState();
    m_trayIcon->updateDeviceList(m_deviceMonitor->connectedDevices());
}

void MainWindow::onDeviceDisconnected(const QString& deviceNode)
{
    DeviceCard* card = getDeviceCard(deviceNode);
    QString deviceName = card ? card->device().displayName() : deviceNode;
    
    logMessage(QString("Device disconnected: %1").arg(deviceName));
    
    // Cancel any pending hash for this device
    for (auto it = m_hashJobDevices.begin(); it != m_hashJobDevices.end(); ++it) {
        if (it.value() == deviceNode) {
            m_hashWorker->cancelHash(it.key());
            break;
        }
    }
    
    removeDeviceCard(deviceNode);
    updateSidebarStats();
    updateEmptyState();
    
    m_trayIcon->notifyDeviceDisconnected(deviceName);
    m_trayIcon->updateDeviceList(m_deviceMonitor->connectedDevices());
}

void MainWindow::onDeviceChanged(const DeviceInfo& device)
{
    updateDeviceCard(device);
}

void MainWindow::onInitialScanComplete(int deviceCount)
{
    logMessage(QString("Initial scan complete: %1 device(s) found").arg(deviceCount), LogLevel::Info);
    updateSidebarStats();
    updateEmptyState();
}

void MainWindow::handleNewDevice(const DeviceInfo& device)
{
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::NewDevice);
    }
    
    if (m_settings.requireConfirmationForNew) {
        bool allowed = showNewDeviceDialog(device);
        
        if (allowed) {
            // Add to database
            DeviceRecord record;
            record.uniqueId = device.uniqueId();
            record.firstSeen = QDateTime::currentDateTime();
            record.lastSeen = record.firstSeen;
            record.trustLevel = m_settings.defaultTrustLevel;
            record.lastKnownInfo = device;
            
            m_database->addDevice(record);
            logMessage(QString("Device whitelisted: %1").arg(device.displayName()));
            
            // Start hashing
            if (m_settings.autoHashOnConnect) {
                startHashing(device.deviceNode);
            }
            
            // Mount the device
            m_mountManager->mount(device.deviceNode);
        } else {
            logMessage(QString("Device rejected: %1").arg(device.displayName()), LogLevel::Warning);
        }
    } else {
        // Auto-whitelist
        DeviceRecord record;
        record.uniqueId = device.uniqueId();
        record.firstSeen = QDateTime::currentDateTime();
        record.lastSeen = record.firstSeen;
        record.trustLevel = m_settings.defaultTrustLevel;
        record.lastKnownInfo = device;
        
        m_database->addDevice(record);
        
        if (m_settings.autoHashOnConnect) {
            startHashing(device.deviceNode);
        }
    }
}

void MainWindow::handleKnownDevice(const DeviceInfo& device, const DeviceRecord& record)
{
    // Update last seen
    m_database->updateLastSeen(record.uniqueId);
    
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setDeviceRecord(record);
    }
    
    if (m_settings.autoHashOnConnect && !record.hash.isEmpty()) {
        // Verify hash
        startHashing(device.deviceNode);
    } else if (m_settings.autoHashOnConnect) {
        // No stored hash, calculate one
        startHashing(device.deviceNode);
    } else {
        // Trusted, mount directly
        if (record.autoMount || record.trustLevel >= 2) {
            m_mountManager->mount(device.deviceNode);
        }
    }
}

// ============================================================================
// Hash Events
// ============================================================================

void MainWindow::onHashStarted(const QString& jobId, const QString& deviceNode)
{
    m_hashJobDevices[jobId] = deviceNode;
    m_activeHashCount++;
    
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setProgressVisible(true);
        card->setVerificationStatus(VerificationStatus::Hashing);
    }
    
    m_trayIcon->setHashingActive(true);
    updateSidebarStats();
}

void MainWindow::onHashProgress(const QString& jobId, double progress, 
                                 quint64 bytesProcessed, double speedMBps)
{
    QString deviceNode = m_hashJobDevices.value(jobId);
    if (deviceNode.isEmpty()) return;
    
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setHashProgress(progress);
        card->setHashSpeed(speedMBps);
    }
    
    // Update status bar
    m_hashStatusLabel->setText(QString("Hashing: %1% @ %2 MB/s")
        .arg(static_cast<int>(progress * 100))
        .arg(speedMBps, 0, 'f', 1));
}

void MainWindow::onHashCompleted(const QString& jobId, const HashResult& result)
{
    QString deviceNode = m_hashJobDevices.take(jobId);
    m_activeHashCount = qMax(0, m_activeHashCount - 1);
    
    if (deviceNode.isEmpty()) return;
    
    DeviceCard* card = getDeviceCard(deviceNode);
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    
    if (!deviceInfo) {
        logMessage(QString("Hash completed but device disconnected: %1").arg(deviceNode));
        return;
    }
    
    QString deviceId = deviceInfo->uniqueId();
    auto record = m_database->getDevice(deviceId);
    
    if (record && !record->hash.isEmpty()) {
        // Verify against stored hash
        if (m_database->verifyHash(deviceId, result.hash)) {
            logMessage(QString("Verified: %1 - hash matches").arg(deviceInfo->displayName()));
            
            if (card) {
                card->setVerificationStatus(VerificationStatus::Verified);
                card->setProgressVisible(false);
                card->flash(FSColor(Verified));
            }
            
            m_trayIcon->notifyVerificationResult(deviceInfo->displayName(), VerificationStatus::Verified);
            
            // Mount if not already
            if (!deviceInfo->isMounted) {
                m_mountManager->mount(deviceNode);
            }
        } else {
            // Hash mismatch!
            logMessage(QString("ALERT: %1 - hash MISMATCH!").arg(deviceInfo->displayName()), LogLevel::Security);
            
            if (card) {
                card->setVerificationStatus(VerificationStatus::Modified);
                card->setProgressVisible(false);
            }
            
            showModifiedDeviceAlert(*deviceInfo, record->hash, result.hash);
            m_trayIcon->notifyVerificationResult(deviceInfo->displayName(), VerificationStatus::Modified);
        }
    } else {
        // First hash for this device
        m_database->updateHash(deviceId, result.hash, result.algorithm, result.durationMs);
        logMessage(QString("Hash stored for %1").arg(deviceInfo->displayName()));
        
        if (card) {
            card->setVerificationStatus(VerificationStatus::Verified);
            card->setProgressVisible(false);
        }
        
        m_trayIcon->notifyHashCompleted(deviceInfo->displayName(), result.durationMs, result.speedMBps());
        
        // Mount if not already
        if (!deviceInfo->isMounted) {
            m_mountManager->mount(deviceNode);
        }
    }
    
    if (m_activeHashCount == 0) {
        m_trayIcon->setHashingActive(false);
        m_hashStatusLabel->clear();
    }
    
    updateSidebarStats();
}

void MainWindow::onHashFailed(const QString& jobId, const QString& error)
{
    QString deviceNode = m_hashJobDevices.take(jobId);
    m_activeHashCount = qMax(0, m_activeHashCount - 1);
    
    logMessage(QString("Hash failed for %1: %2").arg(deviceNode, error), LogLevel::Error);
    
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Error);
        card->setProgressVisible(false);
    }
    
    if (m_activeHashCount == 0) {
        m_trayIcon->setHashingActive(false);
        m_hashStatusLabel->clear();
    }
    
    updateSidebarStats();
}

void MainWindow::onHashCancelled(const QString& jobId)
{
    QString deviceNode = m_hashJobDevices.take(jobId);
    m_activeHashCount = qMax(0, m_activeHashCount - 1);
    
    logMessage(QString("Hash cancelled for %1").arg(deviceNode));
    
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setProgressVisible(false);
    }
    
    if (m_activeHashCount == 0) {
        m_trayIcon->setHashingActive(false);
        m_hashStatusLabel->clear();
    }
    
    updateSidebarStats();
}

// ============================================================================
// Mount Events
// ============================================================================

void MainWindow::onMountCompleted(const MountManager::MountResult& result)
{
    if (result.success) {
        logMessage(QString("Mounted %1 at %2").arg(result.deviceNode, result.mountPoint));
        m_deviceMonitor->rescan();
    } else {
        logMessage(QString("Mount failed for %1: %2").arg(result.deviceNode, result.errorMessage), LogLevel::Error);
    }
}

void MainWindow::onUnmountCompleted(const MountManager::UnmountResult& result)
{
    if (result.success) {
        logMessage(QString("Unmounted %1").arg(result.deviceNode));
        m_deviceMonitor->rescan();
    } else {
        logMessage(QString("Unmount failed for %1: %2").arg(result.deviceNode, result.errorMessage), LogLevel::Error);
    }
}

void MainWindow::onPowerOffCompleted(const QString& deviceNode, bool success, const QString& error)
{
    if (success) {
        logMessage(QString("Device ejected: %1").arg(deviceNode));
    } else {
        logMessage(QString("Eject failed for %1: %2").arg(deviceNode, error), LogLevel::Error);
    }
}

// ============================================================================
// Database Events
// ============================================================================

void MainWindow::onDatabaseLoaded(int deviceCount)
{
    logMessage(QString("Database loaded: %1 whitelisted device(s)").arg(deviceCount));
    updateSidebarStats();
}

void MainWindow::onHashMismatch(const QString& uniqueId, const QString& expected, const QString& actual)
{
    Q_UNUSED(expected)
    Q_UNUSED(actual)
    logMessage(QString("Hash mismatch detected for device %1").arg(uniqueId), LogLevel::Security);
}

// ============================================================================
// Device Card Actions
// ============================================================================

void MainWindow::onMountRequested(const QString& deviceNode)
{
    logMessage(QString("Mount requested: %1").arg(deviceNode));
    m_mountManager->mount(deviceNode);
}

void MainWindow::onUnmountRequested(const QString& deviceNode)
{
    auto device = m_deviceMonitor->getDevice(deviceNode);
    if (!device) return;
    
    // Re-hash before unmount if enabled
    if (m_settings.autoHashOnEject) {
        logMessage(QString("Re-hashing before unmount: %1").arg(deviceNode));
        startHashing(deviceNode);
        // Note: Mount manager will be called after hash completes
    } else {
        m_mountManager->unmount(deviceNode);
    }
}

void MainWindow::onEjectRequested(const QString& deviceNode)
{
    auto device = m_deviceMonitor->getDevice(deviceNode);
    if (!device) return;
    
    logMessage(QString("Eject requested: %1").arg(device->displayName()));
    
    // Unmount first if mounted
    if (device->isMounted) {
        m_mountManager->unmount(deviceNode);
    }
    
    // Power off
    m_mountManager->powerOff(deviceNode);
}

void MainWindow::onRehashRequested(const QString& deviceNode)
{
    logMessage(QString("Rehash requested: %1").arg(deviceNode));
    startHashing(deviceNode);
}

void MainWindow::onOpenMountPointRequested(const QString& mountPoint)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(mountPoint));
}

void MainWindow::onDeviceCardClicked(const QString& deviceNode)
{
    // Could expand card or show details
    Q_UNUSED(deviceNode)
}

// ============================================================================
// Tray Events
// ============================================================================

void MainWindow::onTrayActivated()
{
    toggleVisibility();
}

void MainWindow::onQuitRequested()
{
    m_isClosing = true;
    close();
    QApplication::quit();
}

void MainWindow::onSettingsRequested()
{
    onSettingsClicked();
}

// ============================================================================
// UI Actions
// ============================================================================

void MainWindow::onSearchTextChanged(const QString& text)
{
    QString searchLower = text.toLower();
    
    for (auto it = m_deviceCards.begin(); it != m_deviceCards.end(); ++it) {
        DeviceCard* card = it.value();
        if (text.isEmpty()) {
            card->setVisible(true);
        } else {
            bool matches = card->device().displayName().toLower().contains(searchLower) ||
                           card->device().deviceNode.toLower().contains(searchLower) ||
                           card->device().serial.toLower().contains(searchLower);
            card->setVisible(matches);
        }
    }
}

void MainWindow::onRefreshClicked()
{
    logMessage("Rescanning for devices...");
    m_deviceMonitor->rescan();
    m_mountManager->refreshMountStatus();
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    dialog.loadSettings(m_settings);
    
    connect(&dialog, &SettingsDialog::themeChanged, this, &MainWindow::onThemeChanged);
    
    if (dialog.exec() == QDialog::Accepted) {
        m_settings = dialog.getSettings();
        applySettings(m_settings);
        saveSettings();
    }
}

void MainWindow::onThemeChanged(StyleManager::Theme theme)
{
    FSStyle.setTheme(theme);
    applyStyle();
    
    // Update all device cards
    for (auto* card : m_deviceCards) {
        card->setStyleSheet(FSStyle.deviceCardStyleSheet());
    }
}

void MainWindow::applySettings(const AppSettings& settings)
{
    m_trayIcon->setNotificationsEnabled(settings.showNotifications);
    m_hashWorker->setMaxConcurrent(settings.maxConcurrentHashes);
    FSStyle.setAnimationsEnabled(settings.animationsEnabled);
}

void MainWindow::updateStatusBar()
{
    int connected = m_deviceCards.size();
    int whitelisted = m_database->deviceCount();
    
    m_statusLabel->setText(QString("Connected: %1 | Whitelisted: %2")
        .arg(connected).arg(whitelisted));
}

// ============================================================================
// Helper Methods
// ============================================================================

DeviceCard* MainWindow::addDeviceCard(const DeviceInfo& device)
{
    if (m_deviceCards.contains(device.deviceNode)) {
        return m_deviceCards[device.deviceNode];
    }
    
    DeviceCard* card = new DeviceCard(device);
    
    // Connect card signals
    connect(card, &DeviceCard::mountRequested, this, &MainWindow::onMountRequested);
    connect(card, &DeviceCard::unmountRequested, this, &MainWindow::onUnmountRequested);
    connect(card, &DeviceCard::ejectRequested, this, &MainWindow::onEjectRequested);
    connect(card, &DeviceCard::rehashRequested, this, &MainWindow::onRehashRequested);
    connect(card, &DeviceCard::openMountPointRequested, this, &MainWindow::onOpenMountPointRequested);
    connect(card, &DeviceCard::clicked, this, &MainWindow::onDeviceCardClicked);
    
    // Insert before the stretch
    m_deviceListLayout->insertWidget(m_deviceListLayout->count() - 1, card);
    m_deviceCards[device.deviceNode] = card;
    
    // Note: Don't use FSStyle.applyFadeIn(card) here as DeviceCard 
    // already has its own graphics effect (m_glowEffect) which would be replaced
    
    return card;
}

void MainWindow::removeDeviceCard(const QString& deviceNode)
{
    auto it = m_deviceCards.find(deviceNode);
    if (it != m_deviceCards.end()) {
        DeviceCard* card = it.value();
        m_deviceCards.erase(it);
        card->deleteLater();
    }
}

DeviceCard* MainWindow::getDeviceCard(const QString& deviceNode)
{
    return m_deviceCards.value(deviceNode, nullptr);
}

void MainWindow::updateDeviceCard(const DeviceInfo& device)
{
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setDevice(device);
    }
}

void MainWindow::startHashing(const QString& deviceNode)
{
    HashWorker::HashJob job;
    job.deviceNode = deviceNode;
    job.algorithm = HashWorker::algorithmFromName(m_settings.hashAlgorithm);
    job.bufferSizeKB = m_settings.hashBufferSizeKB;
    job.useMemoryMapping = m_settings.useMemoryMapping;
    
    m_hashWorker->startHash(job);
}

void MainWindow::logMessage(const QString& message, LogLevel level)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString prefix;
    QString color;
    
    switch (level) {
        case LogLevel::Debug:
            prefix = "DEBUG";
            color = FSStyle.colorCss(StyleManager::ColorRole::TextMuted);
            break;
        case LogLevel::Info:
            prefix = "INFO";
            color = FSStyle.colorCss(StyleManager::ColorRole::TextSecondary);
            break;
        case LogLevel::Warning:
            prefix = "WARN";
            color = FSStyle.colorCss(StyleManager::ColorRole::Warning);
            break;
        case LogLevel::Error:
            prefix = "ERROR";
            color = FSStyle.colorCss(StyleManager::ColorRole::Error);
            break;
        case LogLevel::Security:
            prefix = "SECURITY";
            color = FSStyle.colorCss(StyleManager::ColorRole::Modified);
            break;
    }
    
    QString logText = QString("[%1] %2").arg(timestamp, message);
    
    QListWidgetItem* item = new QListWidgetItem(logText);
    item->setForeground(QColor(color));
    m_logList->addItem(item);
    m_logList->scrollToBottom();
    
    // Keep log size reasonable
    while (m_logList->count() > 500) {
        delete m_logList->takeItem(0);
    }
    
    qDebug() << QString("[%1] %2").arg(prefix, message);
}

void MainWindow::updateEmptyState()
{
    if (m_deviceCards.isEmpty()) {
        m_emptyStateLabel->setText(
            "💾\n\nNo USB devices connected\n\n"
            "Connect a USB flash drive to get started.\n"
            "FlashSentry will monitor and verify your devices."
        );
        m_contentStack->setCurrentIndex(1);  // Empty state
    } else {
        m_contentStack->setCurrentIndex(0);  // Device list
    }
}

void MainWindow::updateSidebarStats()
{
    m_connectedCountLabel->setText(QString::number(m_deviceCards.size()));
    m_whitelistedCountLabel->setText(QString::number(m_database->deviceCount()));
    m_hashingCountLabel->setText(QString::number(m_activeHashCount));
    
    m_trayIcon->setDeviceCount(m_deviceCards.size(), m_database->deviceCount());
}

bool MainWindow::showNewDeviceDialog(const DeviceInfo& device)
{
    QString message = QString(
        "<b>Unknown USB device detected:</b><br><br>"
        "<b>Name:</b> %1<br>"
        "<b>Device:</b> %2<br>"
        "<b>Serial:</b> %3<br>"
        "<b>Size:</b> %4<br>"
        "<b>Filesystem:</b> %5<br><br>"
        "Do you want to add this device to the whitelist?")
        .arg(device.displayName())
        .arg(device.deviceNode)
        .arg(device.serial.isEmpty() ? "N/A" : device.serial)
        .arg(device.sizeBytes > 0 ? QString("%1 GB").arg(device.sizeBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1) : "Unknown")
        .arg(device.fsType.isEmpty() ? "Unknown" : device.fsType);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("New Device Detected");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    
    return msgBox.exec() == QMessageBox::Yes;
}

void MainWindow::showModifiedDeviceAlert(const DeviceInfo& device, const QString& expected, const QString& actual)
{
    QString message = QString(
        "<b style='color: red;'>⚠️ SECURITY ALERT ⚠️</b><br><br>"
        "Device <b>%1</b> has been <b>MODIFIED</b> since last use!<br><br>"
        "<b>Device:</b> %2<br>"
        "<b>Expected Hash:</b><br><code>%3</code><br>"
        "<b>Actual Hash:</b><br><code>%4</code><br><br>"
        "This device may have been tampered with. "
        "Do you want to mount it anyway?")
        .arg(device.displayName())
        .arg(device.deviceNode)
        .arg(expected.left(32) + "...")
        .arg(actual.left(32) + "...");
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Security Alert - Device Modified");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Critical);
    
    if (m_settings.blockModifiedDevices) {
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    } else {
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        if (msgBox.exec() == QMessageBox::Yes) {
            // Update hash to new value
            m_database->updateHash(device.uniqueId(), actual, m_settings.hashAlgorithm, 0);
            m_mountManager->mount(device.deviceNode);
            logMessage(QString("User accepted modified device: %1").arg(device.displayName()), LogLevel::Warning);
        }
    }
}

} // namespace FlashSentry