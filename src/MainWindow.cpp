#include "MainWindow.h"
#include "AutostartManager.h"
#include "AuditLog.h"
#include "IsoCatalogManifest.h"
#include "SettingsProfiles.h"
#include "WelcomeWizard.h"
#include "VerifyHistory.h"
#include "HashCheckpoint.h"
#include "HashOptionsDialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QToolButton>
#include <QDateTime>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QScreen>
#include <QStyle>
#include <QGraphicsDropShadowEffect>
#include <QRegularExpression>

namespace FlashSentry {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Initialize settings storage
    m_qsettings = std::make_unique<QSettings>("flashsentry", "FlashSentry");
    
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
    VerifyHistory::instance().load();
    HashCheckpointStore::instance().load();
    
    // Apply styling
    applyStyle();
    refreshVerifyHistoryPanel();
    
    // Setup status update timer
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(STATUS_UPDATE_INTERVAL_MS);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start();
    
    // Start device monitoring
    m_deviceMonitor->startMonitoring();
    configureBadUsbMonitoring();
    
    // Show tray icon
    if (TrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->show();
    }
    
    logMessage("FlashSentry started", LogLevel::Info);

    IsoCatalogManifest::ensureLoaded();
    QTimer::singleShot(0, this, [this]() { warnIfCatalogIntegrityFailed(); });
}

MainWindow::~MainWindow()
{
    m_isClosing = true;
    
    // Stop monitoring
    if (m_deviceMonitor) {
        m_deviceMonitor->stopMonitoring();
    }
    if (m_hidMonitor) {
        m_hidMonitor->stopMonitoring();
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
    setMinimumSize(1024, 680);
    
    // Center on screen
    if (auto* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - 1180) / 2;
        int y = (screenGeometry.height() - 760) / 2;
        setGeometry(x, y, 1180, 760);
    }
    
    // Central widget
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    auto* shell = new QHBoxLayout;
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto* nav = new QWidget;
    nav->setObjectName(QStringLiteral("DashboardNav"));
    nav->setFixedWidth(224);
    auto* navLayout = new QVBoxLayout(nav);
    navLayout->setContentsMargins(12, 16, 12, 12);
    navLayout->setSpacing(8);

    auto* brand = new QLabel(QStringLiteral("🛡  FlashSentry"));
    brand->setObjectName(QStringLiteral("DashboardBrand"));
    brand->setFont(FSFont(Heading2));
    navLayout->addWidget(brand);
    navLayout->addSpacing(16);

    auto addNavButton = [this, navLayout](const QString& text, int index, const QString& tip = {}) {
        auto* button = new QPushButton(text);
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(42);
        button->setToolTip(tip);
        connect(button, &QPushButton::clicked, this, [this, index]() {
            if (index >= 0 && index <= 2) {
                onModeTabChanged(index);
            } else if (index >= 0 && m_appModeStack) {
                m_appModeStack->setCurrentIndex(index);
                updateNavigationState(index);
            } else {
                onSettingsClicked();
            }
        });
        m_navButtons.append(button);
        navLayout->addWidget(button);
    };

    addNavButton(QStringLiteral("▣  USB Monitor"), 0);
    addNavButton(QStringLiteral("◴  Device History"), 3);
    addNavButton(QStringLiteral("☑  Allow / Block List"), 4);
    addNavButton(QStringLiteral("⚠  Alerts"), 5);
    addNavButton(QStringLiteral("▤  Reports"), 1, QStringLiteral("ISO verification and reports"));
    addNavButton(QStringLiteral("⚙  Settings"), -1);
    addNavButton(QStringLiteral("ⓘ  About"), 6);
    navLayout->addStretch();

    auto* protection = new QLabel(QStringLiteral("🛡  Protection Active\nAll systems are protected."));
    protection->setObjectName(QStringLiteral("ProtectionCard"));
    protection->setWordWrap(true);
    protection->setMinimumHeight(72);
    navLayout->addWidget(protection);

    auto* version = new QLabel(QStringLiteral("FlashSentry %1").arg(QApplication::applicationVersion()));
    version->setObjectName(QStringLiteral("NavFooter"));
    navLayout->addWidget(version);
    shell->addWidget(nav);

    auto* contentShell = new QWidget;
    contentShell->setObjectName(QStringLiteral("DashboardContentShell"));
    auto* contentLayout = new QVBoxLayout(contentShell);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_appModeStack = new QStackedWidget;
    m_appModeStack->addWidget(createMainContent());

    m_modeTabBar = new QTabBar(this);
    m_modeTabBar->addTab(QStringLiteral("USB devices"));
    m_modeTabBar->addTab(QStringLiteral("ISO verify"));
    m_modeTabBar->addTab(QStringLiteral("BadUSB"));
    m_modeTabBar->hide();
    connect(m_modeTabBar, &QTabBar::currentChanged, this, &MainWindow::onModeTabChanged);

    m_isoWidget = new IsoVerifierWidget;
    connect(m_isoWidget, &IsoVerifierWidget::logMessageRequested,
            this, &MainWindow::onIsoLogMessage);
    connect(m_isoWidget, &IsoVerifierWidget::verificationReportReady, this,
            &MainWindow::handleIsoVerificationReport);
    connect(m_isoWidget, &IsoVerifierWidget::settingsProfileSelected, this,
            [this](const QString& profileId) {
                SettingsProfiles::applyProfile(profileId, m_settings);
                applySettings(m_settings);
                saveSettings();
                logMessage(QStringLiteral("Profile: %1")
                               .arg(SettingsProfiles::profileDisplayName(profileId)),
                           LogLevel::Info);
            });

    m_appModeStack->addWidget(m_isoWidget);
    m_badUsbWidget = new BadUsbWidget;
    connect(m_badUsbWidget, &BadUsbWidget::logMessageRequested,
            this, &MainWindow::onIsoLogMessage);
    connect(m_badUsbWidget, &BadUsbWidget::trustRequested,
            this, &MainWindow::onBadUsbTrustRequested);
    connect(m_badUsbWidget, &BadUsbWidget::captureRequested,
            this, &MainWindow::onBadUsbCaptureRequested);
    connect(m_badUsbWidget, &BadUsbWidget::refreshRequested, this, [this]() {
        if (m_hidMonitor) {
            m_hidMonitor->rescan();
        }
    });
    connect(m_badUsbWidget, &BadUsbWidget::openCaptureFolderRequested, this, [this]() {
        if (m_usbmonCapture) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_usbmonCapture->outputDirectory()));
        }
    });
    m_appModeStack->addWidget(m_badUsbWidget);
    m_appModeStack->addWidget(createDashboardInfoPage(
        QStringLiteral("Device History"),
        QStringLiteral("Recent USB and HID events are shown on the USB Monitor dashboard. "
                       "A full searchable history view will be backed by the audit log.")));
    m_appModeStack->addWidget(createDashboardInfoPage(
        QStringLiteral("Allow / Block List"),
        QStringLiteral("Known storage devices are managed through the device database. "
                       "BadUSB HID baselines are stored separately with user-confirmed categories. "
                       "Dedicated editing controls will appear here.")));
    m_appModeStack->addWidget(createDashboardInfoPage(
        QStringLiteral("Alerts"),
        QStringLiteral("Security alerts from hash mismatches, manifest mismatches, ISO failures, "
                       "and BadUSB anomalies are summarized in the Recent Events table. "
                       "This page will become the focused alert queue.")));
    m_appModeStack->addWidget(createDashboardInfoPage(
        QStringLiteral("About FlashSentry"),
        QStringLiteral("FlashSentry monitors USB storage, verifies watched content and ISO images, "
                       "and builds BadUSB behavior baselines for HID devices.")));
    contentLayout->addWidget(m_appModeStack, 1);

    auto* footer = new QWidget;
    footer->setObjectName(QStringLiteral("DashboardFooter"));
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(16, 6, 16, 6);
    m_statusLabel = new QLabel(QStringLiteral("Ready"));
    m_footerUserLabel = new QLabel(QStringLiteral("User: Local"));
    m_footerPolicyLabel = new QLabel(QStringLiteral("Policy: Default Policy"));
    m_footerDatabaseLabel = new QLabel(QStringLiteral("●  Database: Connected"));
    m_hashStatusLabel = new QLabel;
    m_catalogStatusBtn = new QPushButton;
    m_catalogStatusBtn->setFlat(true);
    m_catalogStatusBtn->setCursor(Qt::PointingHandCursor);
    connect(m_catalogStatusBtn, &QPushButton::clicked, this, [this]() {
        onModeTabChanged(1);
        if (m_isoWidget) {
            m_isoWidget->refreshCatalogStatus();
        }
    });
    footerLayout->addWidget(m_statusLabel, 1);
    footerLayout->addWidget(m_footerUserLabel);
    footerLayout->addSpacing(28);
    footerLayout->addWidget(m_footerPolicyLabel);
    footerLayout->addSpacing(28);
    footerLayout->addWidget(m_catalogStatusBtn);
    footerLayout->addSpacing(28);
    footerLayout->addWidget(m_hashStatusLabel);
    footerLayout->addSpacing(28);
    footerLayout->addWidget(m_footerDatabaseLabel);
    contentLayout->addWidget(footer);

    shell->addWidget(contentShell, 1);
    m_mainLayout->addLayout(shell, 1);
    
    // Update empty state
    updateEmptyState();
    updateNavigationState(0);
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

    layout->addSpacing(12);

    m_modeTabBar = new QTabBar;
    m_modeTabBar->addTab(QStringLiteral("USB devices"));
    m_modeTabBar->addTab(QStringLiteral("ISO verify"));
    m_modeTabBar->addTab(QStringLiteral("BadUSB"));
    m_modeTabBar->setDocumentMode(true);
    m_modeTabBar->setExpanding(false);
    m_modeTabBar->setStyleSheet(FSStyle.tabWidgetStyleSheet());
    connect(m_modeTabBar, &QTabBar::currentChanged, this, &MainWindow::onModeTabChanged);
    layout->addWidget(m_modeTabBar);

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
    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("DashboardScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("UsbDashboardPage"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;
    auto* titleBlock = new QVBoxLayout;
    auto* title = new QLabel(QStringLiteral("▣  USB Monitor"));
    title->setObjectName(QStringLiteral("PageTitle"));
    title->setFont(FSFont(Heading1));
    auto* subtitle = new QLabel(QStringLiteral("Real-time monitoring of USB devices and storage activities."));
    subtitle->setObjectName(QStringLiteral("PageSubtitle"));
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);
    header->addLayout(titleBlock, 1);

    m_refreshBtn = new QPushButton(QStringLiteral("⟳  Refresh"));
    m_refreshBtn->setObjectName(QStringLiteral("DashboardButton"));
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    header->addWidget(m_refreshBtn);

    m_monitoringStatusLabel = new QLabel(QStringLiteral("●  Monitoring Active"));
    m_monitoringStatusLabel->setObjectName(QStringLiteral("MonitoringPill"));
    header->addWidget(m_monitoringStatusLabel);
    layout->addLayout(header);

    auto* cards = new QHBoxLayout;
    cards->setSpacing(14);
    auto makeCard = [this](const QString& icon, const QString& title, const QString& detail,
                           QLabel*& value, const QString& accent) {
        auto* card = new QWidget;
        card->setObjectName(QStringLiteral("StatCard"));
        card->setProperty("accent", accent);
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(18, 16, 18, 14);
        cardLayout->setSpacing(14);
        auto* iconLabel = new QLabel(icon);
        iconLabel->setObjectName(QStringLiteral("StatIcon"));
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setFixedSize(48, 48);
        cardLayout->addWidget(iconLabel);
        auto* text = new QVBoxLayout;
        value = new QLabel(QStringLiteral("0"));
        value->setObjectName(QStringLiteral("StatValue"));
        value->setFont(FSFont(Heading1));
        auto* label = new QLabel(title);
        label->setObjectName(QStringLiteral("StatTitle"));
        auto* sub = new QLabel(detail);
        sub->setObjectName(QStringLiteral("StatDetail"));
        text->addWidget(value);
        text->addWidget(label);
        text->addWidget(sub);
        cardLayout->addLayout(text, 1);
        return card;
    };
    cards->addWidget(makeCard(QStringLiteral("USB"), QStringLiteral("Connected"),
                              QStringLiteral("USB device(s)"), m_connectedCountLabel,
                              QStringLiteral("blue")));
    cards->addWidget(makeCard(QStringLiteral("✓"), QStringLiteral("Allowed"),
                              QStringLiteral("This session"), m_whitelistedCountLabel,
                              QStringLiteral("green")));
    cards->addWidget(makeCard(QStringLiteral("!"), QStringLiteral("Blocked"),
                              QStringLiteral("This session"), m_blockedCountLabel,
                              QStringLiteral("orange")));
    cards->addWidget(makeCard(QStringLiteral("▤"), QStringLiteral("Total Events"),
                              QStringLiteral("This session"), m_totalEventsLabel,
                              QStringLiteral("purple")));
    layout->addLayout(cards);

    auto makePanelTitle = [](const QString& text) {
        auto* label = new QLabel(text);
        label->setObjectName(QStringLiteral("PanelTitle"));
        return label;
    };

    auto* devicePanel = new QWidget;
    devicePanel->setObjectName(QStringLiteral("DashboardPanel"));
    auto* devicePanelLayout = new QVBoxLayout(devicePanel);
    devicePanelLayout->setContentsMargins(0, 0, 0, 0);
    devicePanelLayout->setSpacing(0);
    devicePanelLayout->addWidget(makePanelTitle(QStringLiteral("Connected Devices")));

    m_deviceTable = new QTableWidget(0, 7);
    m_deviceTable->setObjectName(QStringLiteral("DashboardTable"));
    m_deviceTable->setHorizontalHeaderLabels({
        QStringLiteral("Device Name"),
        QStringLiteral("Type"),
        QStringLiteral("Status"),
        QStringLiteral("Capacity"),
        QStringLiteral("Vendor / Model"),
        QStringLiteral("Connected At"),
        QStringLiteral("Actions"),
    });
    m_deviceTable->verticalHeader()->setVisible(false);
    m_deviceTable->horizontalHeader()->setStretchLastSection(true);
    m_deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deviceTable->setMinimumHeight(180);
    devicePanelLayout->addWidget(m_deviceTable);
    layout->addWidget(devicePanel);

    auto* eventsPanel = new QWidget;
    eventsPanel->setObjectName(QStringLiteral("DashboardPanel"));
    auto* eventsLayout = new QVBoxLayout(eventsPanel);
    eventsLayout->setContentsMargins(0, 0, 0, 0);
    eventsLayout->setSpacing(0);
    eventsLayout->addWidget(makePanelTitle(QStringLiteral("Recent Events")));

    m_eventTable = new QTableWidget(0, 6);
    m_eventTable->setObjectName(QStringLiteral("DashboardTable"));
    m_eventTable->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Event"),
        QStringLiteral("Device"),
        QStringLiteral("Type"),
        QStringLiteral("Result"),
        QStringLiteral("Details"),
    });
    m_eventTable->verticalHeader()->setVisible(false);
    m_eventTable->horizontalHeader()->setStretchLastSection(true);
    m_eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventTable->setMinimumHeight(210);
    eventsLayout->addWidget(m_eventTable);
    layout->addWidget(eventsPanel, 1);

    m_logList = new QListWidget;
    m_logList->hide();
    m_deviceListWidget = new QWidget;
    m_deviceListLayout = new QVBoxLayout(m_deviceListWidget);
    m_deviceListLayout->addStretch();
    m_contentStack = new QStackedWidget;
    m_emptyStateLabel = new QLabel;
    m_contentStack->addWidget(m_deviceListWidget);
    m_contentStack->addWidget(m_emptyStateLabel);

    scroll->setWidget(page);
    m_splitter = scroll;
    return scroll;
}

QWidget* MainWindow::createDashboardInfoPage(const QString& title, const QString& body)
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("UsbDashboardPage"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(18);

    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("PageTitle"));
    titleLabel->setFont(FSFont(Heading1));
    layout->addWidget(titleLabel);

    auto* panel = new QWidget;
    panel->setObjectName(QStringLiteral("DashboardPanel"));
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 18, 18, 18);
    auto* text = new QLabel(body);
    text->setWordWrap(true);
    text->setObjectName(QStringLiteral("PageSubtitle"));
    panelLayout->addWidget(text);
    layout->addWidget(panel);
    layout->addStretch();
    return page;
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

    m_historyFilterLabel = new QLabel(QStringLiteral("Verify history"));
    m_historyFilterLabel->setFont(FSFont(Heading3));
    m_historyFilterLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(m_historyFilterLabel);

    m_historyList = new QListWidget;
    m_historyList->setFrameShape(QFrame::NoFrame);
    m_historyList->setMaximumHeight(140);
    m_historyList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_historyList->setStyleSheet(FSStyle.listWidgetStyleSheet());
    m_historyList->setToolTip(QStringLiteral("Recent verification results. Click a device card to filter."));
    connect(m_historyList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QString node = item->data(Qt::UserRole).toString();
        if (!node.isEmpty() && m_modeTabBar) {
            auto info = m_deviceMonitor->getDevice(node);
            if (info && !info->mountPoint.isEmpty()) {
                m_modeTabBar->setCurrentIndex(1);
                onModeTabChanged(1);
                if (m_isoWidget) {
                    m_isoWidget->focusDevice(node, info->mountPoint, info->displayName());
                }
            }
        }
    });
    layout->addWidget(m_historyList);
    
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

    m_catalogStatusBtn = new QPushButton;
    m_catalogStatusBtn->setFlat(true);
    m_catalogStatusBtn->setCursor(Qt::PointingHandCursor);
    m_catalogStatusBtn->setStyleSheet(QString("color: %1; text-align: left;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    connect(m_catalogStatusBtn, &QPushButton::clicked, this, [this]() {
        if (m_modeTabBar) {
            m_modeTabBar->setCurrentIndex(1);
        }
        m_settings.appModule = AppModule::IsoVerifier;
        applyAppModule();
        saveSettings();
        if (m_isoWidget) {
            m_isoWidget->refreshCatalogStatus();
        }
    });
    status->addPermanentWidget(m_catalogStatusBtn);

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

    m_manifestWorker = std::make_unique<ManifestWorker>(this);
    
    // Create database manager
    m_database = std::make_unique<DatabaseManager>(this);
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) 
                     + "/flashsentry/devices.json";
    m_database->initialize(dbPath);
    
    // Create mount manager
    m_mountManager = std::make_unique<MountManager>(this);
    
    // Create tray icon
    m_trayIcon = std::make_unique<TrayIcon>(this);

    m_hidMonitor = std::make_unique<HidDeviceMonitor>(this);
    m_badUsbBaselineStore = std::make_unique<BadUsbBaselineStore>(this);
    m_badUsbBaselineStore->initialize();
    m_usbmonCapture = std::make_unique<UsbmonCapture>(this);
    if (m_badUsbWidget) {
        m_badUsbWidget->setBaselineCount(m_badUsbBaselineStore->allDevices().size());
    }
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
    connect(m_deviceMonitor.get(), &DeviceMonitor::monitorError,
            this, [this](const QString& error) {
                logMessage(QString("Device monitor error: %1").arg(error), LogLevel::Error);
            });

    connect(m_hidMonitor.get(), &HidDeviceMonitor::hidConnected,
            this, &MainWindow::onHidConnected);
    connect(m_hidMonitor.get(), &HidDeviceMonitor::hidDisconnected,
            this, &MainWindow::onHidDisconnected);
    connect(m_hidMonitor.get(), &HidDeviceMonitor::hidChanged,
            this, &MainWindow::onHidChanged);
    connect(m_hidMonitor.get(), &HidDeviceMonitor::monitorError, this, [this](const QString& error) {
        logMessage(QStringLiteral("BadUSB monitor error: %1").arg(error), LogLevel::Error);
    });
    connect(m_hidMonitor.get(), &HidDeviceMonitor::initialScanComplete, this, [this](int count) {
        logMessage(QStringLiteral("BadUSB HID scan complete: %1 device(s)").arg(count), LogLevel::Info);
        if (m_badUsbWidget) {
            m_badUsbWidget->setDevices({});
            for (const HidDeviceInfo& info : m_hidMonitor->connectedDevices()) {
                const auto baseline = m_badUsbBaselineStore->getDevice(info.stableId());
                m_badUsbWidget->updateDevice(info, baseline.has_value() && baseline->trusted);
            }
        }
    });
    connect(m_badUsbBaselineStore.get(), &BadUsbBaselineStore::baselineChanged, this, [this]() {
        if (m_badUsbWidget) {
            m_badUsbWidget->setBaselineCount(m_badUsbBaselineStore->allDevices().size());
        }
    });
    connect(m_usbmonCapture.get(), &UsbmonCapture::captureStarted, this, [this](const QString& path) {
        logMessage(QStringLiteral("BadUSB usbmon capture started: %1").arg(path), LogLevel::Security);
        if (m_badUsbWidget) m_badUsbWidget->setCaptureStatus(path);
    });
    connect(m_usbmonCapture.get(), &UsbmonCapture::captureFinished, this,
            [this](const QString& path, int exitCode) {
                logMessage(QStringLiteral("BadUSB usbmon capture finished (%1): %2").arg(exitCode).arg(path),
                           LogLevel::Info);
                if (m_badUsbWidget) m_badUsbWidget->setCaptureStatus(QStringLiteral("finished"));
            });
    connect(m_usbmonCapture.get(), &UsbmonCapture::captureFailed, this, [this](const QString& error) {
        logMessage(QStringLiteral("BadUSB usbmon capture failed: %1").arg(error), LogLevel::Warning);
        if (m_badUsbWidget) m_badUsbWidget->setCaptureStatus(error);
    });
    
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
    

    connect(m_manifestWorker.get(), &ManifestWorker::manifestStarted,
            this, &MainWindow::onManifestStarted);
    connect(m_manifestWorker.get(), &ManifestWorker::manifestCompleted,
            this, &MainWindow::onManifestCompleted);
    connect(m_manifestWorker.get(), &ManifestWorker::manifestBaselineBuilt,
            this, &MainWindow::onManifestBaselineBuilt);
    connect(m_manifestWorker.get(), &ManifestWorker::manifestFailed,
            this, &MainWindow::onManifestFailed);

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
    connect(m_trayIcon.get(), &TrayIcon::auditLogOpenRequested, this, [this]() {
        const QString path = AuditLog::logPath();
        if (!QFileInfo::exists(path)) {
            logMessage(QStringLiteral("Audit log not created yet: %1").arg(path), LogLevel::Info);
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(m_trayIcon.get(), &TrayIcon::deviceEjectRequested,
            this, &MainWindow::onEjectRequested);
}

void MainWindow::loadSettings()
{
    m_settings.startMinimized = m_qsettings->value("general/startMinimized", false).toBool();
    m_settings.autoStartAtLogin = m_qsettings->value("general/autoStartAtLogin", false).toBool();
    m_settings.minimizeToTray = m_qsettings->value("general/minimizeToTray", true).toBool();
    m_settings.showNotifications = m_qsettings->value("general/showNotifications", true).toBool();
    m_settings.autoHashOnConnect = m_qsettings->value("security/autoHashOnConnect", false).toBool();
    m_settings.autoHashOnEject = m_qsettings->value("security/autoHashOnEject", true).toBool();
    m_settings.appModule = appModuleFromString(m_qsettings->value("general/appModule", "usb_monitor").toString());
    m_settings.defaultVerificationProfile = verificationProfileFromString(
        m_qsettings->value("security/defaultVerificationProfile", "watch_manifest").toString());
    m_settings.isoScanDirectory = m_qsettings->value("iso/scanDirectory").toString();
    m_settings.isoAutoVerifyOnScan = m_qsettings->value("iso/autoVerify", true).toBool();
    m_settings.isoAutoVerifyOnUsbMount = m_qsettings->value("iso/autoVerifyOnUsbMount", true).toBool();
    m_settings.blockMountOnIsoVerifyFailure =
        m_qsettings->value("iso/blockMountOnFailure", false).toBool();
    m_settings.isoVerifyDecompressed = m_qsettings->value("iso/verifyDecompressed", false).toBool();
    m_settings.isoPreferOfflineSidecars = m_qsettings->value("iso/preferOfflineSidecars", false).toBool();
    m_settings.isoVerifyParallel = m_qsettings->value("iso/verifyParallel", 2).toInt();
    m_settings.showFirstRunWizard = m_qsettings->value("general/showFirstRunWizard", true).toBool();
    m_settings.badUsbEnabled = m_qsettings->value("badusb/enabled", true).toBool();
    m_settings.badUsbAlertNewKeyboard =
        m_qsettings->value("badusb/alertNewKeyboard", true).toBool();
    m_settings.badUsbAlertCompositeStorage =
        m_qsettings->value("badusb/alertCompositeStorage", true).toBool();
    m_settings.badUsbAlertInterfaceDrift =
        m_qsettings->value("badusb/alertInterfaceDrift", true).toBool();
    m_settings.badUsbAlertRapidReconnect =
        m_qsettings->value("badusb/alertRapidReconnect", true).toBool();
    m_settings.badUsbAutoBaselineTrusted =
        m_qsettings->value("badusb/autoBaselineTrusted", false).toBool();
    m_settings.badUsbConfirmAnomalies =
        m_qsettings->value("badusb/confirmAnomalies", true).toBool();
    m_settings.badUsbUsbmonEnabled =
        m_qsettings->value("badusb/usbmonEnabled", false).toBool();
    m_settings.badUsbUsbmonOnAnomalyOnly =
        m_qsettings->value("badusb/usbmonOnAnomalyOnly", true).toBool();
    m_settings.badUsbUsbmonCommand =
        m_qsettings->value("badusb/usbmonCommand",
                           QStringLiteral("tcpdump -i usbmon{bus} -w {out} -G 30 -W 1")).toString();
    {
        const QString storedProfile =
            m_qsettings->value("general/settingsProfile", QStringLiteral("default")).toString();
        m_settings.settingsProfile = SettingsProfiles::normalizeProfileId(storedProfile);
        if (storedProfile == QStringLiteral("ventoy")) {
            m_qsettings->setValue("general/settingsProfile", QStringLiteral("multi_image"));
        }
    }
    m_settings.requireConfirmationForNew = m_qsettings->value("security/confirmNewDevice", true).toBool();
    m_settings.requireConfirmationForModified = m_qsettings->value("security/confirmModified", true).toBool();
    m_settings.blockModifiedDevices = m_qsettings->value("security/blockModified", false).toBool();
    m_settings.hashAlgorithm = m_qsettings->value("hashing/algorithm", "SHA256").toString();
    m_settings.hashBufferSizeKB = m_qsettings->value("hashing/bufferSizeKB", 1024).toInt();
    m_settings.useMemoryMapping = m_qsettings->value("hashing/useMemoryMapping", true).toBool();
    m_settings.maxConcurrentHashes = m_qsettings->value("hashing/maxConcurrent", 1).toInt();
    m_settings.defaultHashScope = hashScopeFromString(
        m_qsettings->value("hashing/defaultScope", "partition").toString());
    m_settings.defaultHashScanMode = hashScanModeFromString(
        m_qsettings->value("hashing/defaultScanMode", "full").toString());
    m_settings.hashResumeCheckpoints = m_qsettings->value("hashing/resumeCheckpoints", true).toBool();
    m_settings.promptHashOptionsOnManual = m_qsettings->value("hashing/promptOnManual", true).toBool();
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
    applyAppModule();
    applyIsoVerifyOptions();
    configureBadUsbMonitoring();

    // Restore window geometry
    if (m_qsettings->contains("window/geometry")) {
        restoreGeometry(m_qsettings->value("window/geometry").toByteArray());
    }
}

void MainWindow::saveSettings()
{
    m_qsettings->setValue("general/startMinimized", m_settings.startMinimized);
    m_qsettings->setValue("general/autoStartAtLogin", m_settings.autoStartAtLogin);
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
    m_qsettings->setValue("hashing/defaultScope", hashScopeToString(m_settings.defaultHashScope));
    m_qsettings->setValue("hashing/defaultScanMode", hashScanModeToString(m_settings.defaultHashScanMode));
    m_qsettings->setValue("hashing/resumeCheckpoints", m_settings.hashResumeCheckpoints);
    m_qsettings->setValue("hashing/promptOnManual", m_settings.promptHashOptionsOnManual);
    m_qsettings->setValue("appearance/animations", m_settings.animationsEnabled);
    m_qsettings->setValue("general/appModule", appModuleToString(m_settings.appModule));
    m_qsettings->setValue("security/defaultVerificationProfile", verificationProfileToString(m_settings.defaultVerificationProfile));
    m_qsettings->setValue("iso/scanDirectory", m_settings.isoScanDirectory);
    m_qsettings->setValue("iso/autoVerify", m_settings.isoAutoVerifyOnScan);
    m_qsettings->setValue("iso/autoVerifyOnUsbMount", m_settings.isoAutoVerifyOnUsbMount);
    m_qsettings->setValue("iso/blockMountOnFailure", m_settings.blockMountOnIsoVerifyFailure);
    m_qsettings->setValue("iso/verifyDecompressed", m_settings.isoVerifyDecompressed);
    m_qsettings->setValue("iso/preferOfflineSidecars", m_settings.isoPreferOfflineSidecars);
    m_qsettings->setValue("iso/verifyParallel", m_settings.isoVerifyParallel);
    m_qsettings->setValue("general/showFirstRunWizard", m_settings.showFirstRunWizard);
    m_qsettings->setValue("general/settingsProfile", m_settings.settingsProfile);
    m_qsettings->setValue("badusb/enabled", m_settings.badUsbEnabled);
    m_qsettings->setValue("badusb/alertNewKeyboard", m_settings.badUsbAlertNewKeyboard);
    m_qsettings->setValue("badusb/alertCompositeStorage", m_settings.badUsbAlertCompositeStorage);
    m_qsettings->setValue("badusb/alertInterfaceDrift", m_settings.badUsbAlertInterfaceDrift);
    m_qsettings->setValue("badusb/alertRapidReconnect", m_settings.badUsbAlertRapidReconnect);
    m_qsettings->setValue("badusb/autoBaselineTrusted", m_settings.badUsbAutoBaselineTrusted);
    m_qsettings->setValue("badusb/confirmAnomalies", m_settings.badUsbConfirmAnomalies);
    m_qsettings->setValue("badusb/usbmonEnabled", m_settings.badUsbUsbmonEnabled);
    m_qsettings->setValue("badusb/usbmonOnAnomalyOnly", m_settings.badUsbUsbmonOnAnomalyOnly);
    m_qsettings->setValue("badusb/usbmonCommand", m_settings.badUsbUsbmonCommand);
    m_qsettings->setValue("appearance/theme", FSStyle.themeName(FSStyle.currentTheme()));
    m_qsettings->setValue("window/geometry", saveGeometry());
    
    m_qsettings->sync();
}

void MainWindow::applyStyle()
{
    const QString bg = FSStyle.colorCss(StyleManager::ColorRole::Background);
    const QString bgAlt = FSStyle.colorCss(StyleManager::ColorRole::BackgroundAlt);
    const QString surface = FSStyle.colorCss(StyleManager::ColorRole::Surface);
    const QString hover = FSStyle.colorCss(StyleManager::ColorRole::SurfaceHover);
    const QString border = FSStyle.colorCss(StyleManager::ColorRole::Border);
    const QString text = FSStyle.colorCss(StyleManager::ColorRole::TextPrimary);
    const QString muted = FSStyle.colorCss(StyleManager::ColorRole::TextMuted);
    const QString secondary = FSStyle.colorCss(StyleManager::ColorRole::TextSecondary);
    const QString accent = FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary);
    const QString success = FSStyle.colorCss(StyleManager::ColorRole::Success);
    const QString warning = FSStyle.colorCss(StyleManager::ColorRole::Warning);

    setStyleSheet(QString(R"(
        QMainWindow, QWidget#DashboardContentShell, QWidget#UsbDashboardPage {
            background: %1;
            color: %6;
        }
        QWidget#DashboardNav {
            background: #0a111a;
            border-right: 1px solid %5;
        }
        QLabel#DashboardBrand {
            color: %6;
            font-weight: 700;
            padding: 4px 6px;
        }
        QLabel#NavFooter {
            color: %7;
            padding: 8px 4px;
        }
        QLabel#ProtectionCard {
            background: rgba(36, 48, 60, 0.75);
            border: 1px solid %5;
            border-radius: 8px;
            color: %10;
            padding: 12px;
        }
        QPushButton#NavButton {
            text-align: left;
            padding: 10px 12px;
            border: 0;
            border-radius: 7px;
            color: %8;
            background: transparent;
        }
        QPushButton#NavButton:hover {
            background: %4;
            color: %6;
        }
        QPushButton#NavButton:checked {
            background: #0d6efd;
            color: white;
            font-weight: 600;
        }
        QLabel#PageTitle {
            color: %6;
        }
        QLabel#PageSubtitle {
            color: %8;
        }
        QPushButton#DashboardButton {
            background: %3;
            border: 1px solid %5;
            border-radius: 8px;
            padding: 8px 14px;
            color: %6;
        }
        QLabel#MonitoringPill {
            color: %10;
            font-weight: 600;
            padding: 8px 10px;
        }
        QWidget#StatCard {
            background: %3;
            border: 1px solid %5;
            border-radius: 8px;
            border-bottom: 3px solid %9;
        }
        QLabel#StatIcon {
            background: rgba(13, 110, 253, 0.22);
            border-radius: 24px;
            color: %9;
            font-weight: 700;
        }
        QLabel#StatValue {
            color: %6;
        }
        QLabel#StatTitle {
            color: %6;
            font-weight: 600;
        }
        QLabel#StatDetail {
            color: %7;
        }
        QWidget#DashboardPanel {
            background: %3;
            border: 1px solid %5;
            border-radius: 8px;
        }
        QLabel#PanelTitle {
            color: %6;
            font-weight: 700;
            padding: 12px 14px;
            border-bottom: 1px solid %5;
        }
        QTableWidget#DashboardTable {
            background: transparent;
            border: 0;
            gridline-color: %5;
            color: %8;
            selection-background-color: rgba(13, 110, 253, 0.25);
        }
        QTableWidget#DashboardTable::item {
            padding: 8px;
            border-bottom: 1px solid rgba(255,255,255,0.04);
        }
        QToolButton#DashboardActionButton {
            background: %2;
            color: %6;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 6px 10px;
        }
        QToolButton#DashboardActionButton::menu-indicator {
            image: none;
        }
        QHeaderView::section {
            background: %2;
            color: %8;
            border: 0;
            border-bottom: 1px solid %5;
            padding: 8px;
            font-weight: 600;
        }
        QWidget#DashboardFooter {
            background: %2;
            border-top: 1px solid %5;
            color: %8;
        }
        QScrollArea#DashboardScroll {
            border: 0;
            background: %1;
        }
    )").arg(bg, bgAlt, surface, hover, border, text, muted, secondary, accent, success, warning));

    if (m_logList) {
        m_logList->setStyleSheet(FSStyle.listWidgetStyleSheet());
    }
    updateNavigationState(m_modeTabBar ? m_modeTabBar->currentIndex() : 0);
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

void MainWindow::showSettingsDialog()
{
    onSettingsClicked();
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

    static bool wizardShown = false;
    if (!wizardShown && m_settings.showFirstRunWizard) {
        wizardShown = true;
        WelcomeWizard wizard(this);
        if (wizard.exec() == QDialog::Accepted) {
            wizard.applyToSettings(m_settings);
            applySettings(m_settings);
            saveSettings();
            if (m_isoWidget) {
                m_isoWidget->setActiveProfile(m_settings.settingsProfile);
            }
        }
    }
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
    addDeviceCard(device);
    
    // Check if device is known
    if (m_database->hasDevice(device)) {
        auto record = m_database->getDevice(device);
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
    maybeTriggerIsoVerifyForMountedDevice(device);
}

void MainWindow::onDeviceDisconnected(const QString& deviceNode)
{
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        clearIsoVerifyDedupForDevice(card->device());
    }
    QString deviceName = card ? card->device().displayName() : deviceNode;
    QString drive;
    if (card) {
        drive = driveKey(card->device());
    }

    logMessage(QString("Device disconnected: %1").arg(deviceName));
    
    // Cancel any pending hash for this device
    for (auto it = m_hashJobDevices.begin(); it != m_hashJobDevices.end(); ++it) {
        if (it.value() == deviceNode) {
            m_hashWorker->cancelHash(it.key());
            break;
        }
    }
    
    removeDeviceCard(deviceNode);
    m_pendingHashActions.remove(deviceNode);
    m_lastVerificationHashes.remove(deviceNode);

    if (!drive.isEmpty()) {
        bool driveStillPresent = false;
        for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
            if (driveKey(d) == drive) {
                driveStillPresent = true;
                break;
            }
        }
        if (!driveStillPresent) {
            m_rejectedDrives.remove(drive);
            m_drivePromptInProgress.remove(drive);
        }
    }

    updateSidebarStats();
    updateEmptyState();
    
    m_trayIcon->notifyDeviceDisconnected(deviceName);
    m_trayIcon->updateDeviceList(m_deviceMonitor->connectedDevices());
}

void MainWindow::onDeviceChanged(const DeviceInfo& device)
{
    updateDeviceCard(device);
    maybeTriggerIsoVerifyForMountedDevice(device);
}

void MainWindow::onInitialScanComplete(int deviceCount)
{
    logMessage(QString("Initial scan complete: %1 device(s) found").arg(deviceCount), LogLevel::Info);
    updateSidebarStats();
    updateEmptyState();
    for (const DeviceInfo& device : m_deviceMonitor->connectedDevices()) {
        maybeTriggerIsoVerifyForMountedDevice(device);
    }
}

void MainWindow::handleNewDevice(const DeviceInfo& device)
{
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::NewDevice);
    }

    if (m_settings.promptPerPartition) {
        handleNewDevicePartition(device);
        return;
    }

    const QString drive = driveKey(device);
    if (m_rejectedDrives.contains(drive)) {
        logMessage(QString("Drive rejected (earlier): %1").arg(device.displayName()), LogLevel::Warning);
        return;
    }

    if (isDriveKnown(device)) {
        handleNewDevicePartition(device);
        return;
    }

    if (m_drivePromptInProgress.contains(drive)) {
        return;
    }

    if (!m_settings.requireConfirmationForNew) {
        whitelistDrivePartitions(device);
        return;
    }

    m_drivePromptInProgress.insert(drive);
    const bool allowed = showNewDriveDialog(device);
    m_drivePromptInProgress.remove(drive);

    if (allowed) {
        whitelistDrivePartitions(device);
    } else {
        m_rejectedDrives.insert(drive);
        logMessage(QString("Drive rejected: %1").arg(device.displayName()), LogLevel::Warning);
    }
}

void MainWindow::handleNewDevicePartition(const DeviceInfo& device)
{
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::NewDevice);
    }

    if (m_settings.requireConfirmationForNew) {
        if (!showNewDeviceDialog(device)) {
            logMessage(QString("Device rejected: %1").arg(device.displayName()), LogLevel::Warning);
            return;
        }
    }

    DeviceRecord record;
    record.uniqueId = device.uniqueId();
    record.firstSeen = QDateTime::currentDateTime();
    record.lastSeen = record.firstSeen;
    record.trustLevel = m_settings.defaultTrustLevel;
    record.lastKnownInfo = device;

    m_database->addDevice(record);
    logMessage(QString("Device whitelisted: %1").arg(device.displayName()));

    if (m_settings.autoHashOnConnect) {
        startDeviceVerification(device.deviceNode);
        hashAllPartitionsOnParent(device);
    }
}

QString MainWindow::driveKey(const DeviceInfo& device) const
{
    if (!device.parentDevice.isEmpty()) {
        return device.parentDevice;
    }
    return device.deviceNode.section('/', -1);
}

bool MainWindow::isDriveKnown(const DeviceInfo& device) const
{
    const QString drive = driveKey(device);
    for (const DeviceInfo& part : m_deviceMonitor->connectedDevices()) {
        if (driveKey(part) != drive) {
            continue;
        }
        if (m_database->hasDevice(part)) {
            return true;
        }
    }
    return false;
}

void MainWindow::whitelistDrivePartitions(const DeviceInfo& device)
{
    const QString drive = driveKey(device);
    for (const DeviceInfo& part : m_deviceMonitor->connectedDevices()) {
        if (driveKey(part) != drive) {
            continue;
        }
        if (m_database->hasDevice(part)) {
            continue;
        }

        DeviceRecord record;
        record.uniqueId = part.uniqueId();
        record.firstSeen = QDateTime::currentDateTime();
        record.lastSeen = record.firstSeen;
        record.trustLevel = m_settings.defaultTrustLevel;
        record.lastKnownInfo = part;

        m_database->addDevice(record);

        DeviceCard* card = getDeviceCard(part.deviceNode);
        if (card) {
            card->setVerificationStatus(VerificationStatus::Pending);
        }
    }

    logMessage(QString("Drive whitelisted: %1").arg(drive));

    if (m_settings.autoHashOnConnect) {
        for (const DeviceInfo& part : m_deviceMonitor->connectedDevices()) {
            if (driveKey(part) != drive) {
                continue;
            }
            if (!m_database->hasDevice(part)) {
                continue;
            }
            if (m_hashJobDevices.values().contains(part.deviceNode)) {
                continue;
            }
            startHashing(part.deviceNode);
        }
    }
}

void MainWindow::handleKnownDevice(const DeviceInfo& device, const DeviceRecord& record)
{
    const QString deviceId = m_database->canonicalUniqueId(device);
    m_database->updateLastSeen(deviceId);
    
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        if (auto updated = m_database->getDevice(deviceId)) {
            card->setDeviceRecord(*updated);
        } else {
            card->setDeviceRecord(record);
        }
    }
    
    if (m_settings.autoHashOnConnect) {
        startDeviceVerification(device.deviceNode);
        hashAllPartitionsOnParent(device);
    }
}

// ============================================================================
// Hash Events
// ============================================================================

void MainWindow::onHashStarted(const QString& jobId, const QString& deviceNode)
{
    if (!m_hashJobDevices.contains(jobId)) {
        m_hashJobDevices[jobId] = deviceNode;
    }
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
                                 quint64 bytesProcessed, double speedMBps,
                                 double etaSeconds, quint64 totalBytes)
{
    QString deviceNode = m_hashJobDevices.value(jobId);
    if (deviceNode.isEmpty()) return;
    
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setHashProgress(progress);
        card->setHashSpeed(speedMBps);
        card->setHashEta(etaSeconds);
        card->setHashBytes(bytesProcessed, totalBytes);
    }

    QString etaText;
    if (etaSeconds > 1.0) {
        const int mins = static_cast<int>(etaSeconds) / 60;
        const int secs = static_cast<int>(etaSeconds) % 60;
        etaText = mins > 0 ? QString(" · ETA %1m %2s").arg(mins).arg(secs, 2, 10, QChar('0'))
                           : QString(" · ETA %1s").arg(secs);
    }
    m_hashStatusLabel->setText(QString("Hashing: %1% @ %2 MB/s%3")
        .arg(static_cast<int>(progress * 100))
        .arg(speedMBps, 0, 'f', 1)
        .arg(etaText));
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
        m_pendingHashActions.remove(deviceNode);
        return;
    }
    
    const PendingHashAction pending = m_pendingHashActions.take(deviceNode);
    const HashJobContext ctx = m_hashJobContext.value(jobId);
    m_hashJobContext.remove(jobId);
    const QString deviceId = ctx.storageId.isEmpty() ? canonicalDeviceId(*deviceInfo) : ctx.storageId;
    auto record = m_database->getDevice(deviceId);
    
    auto finishVerified = [&]() {
        if (pending == PendingHashAction::UnmountAfterVerify) {
            m_mountManager->unmount(deviceNode);
            return;
        }
        if (deviceInfo->isMounted) {
            return;
        }
        if (pending == PendingHashAction::MountAfterVerify) {
            m_mountManager->mount(deviceNode);
        } else {
            mountIfVerified(deviceNode);
        }
    };
    
    if (record && !record->hash.isEmpty()) {
        if (m_database->verifyHash(canonicalDeviceId(*deviceInfo), result.hash)) {
            logMessage(QString("Verified: %1 - hash matches").arg(deviceInfo->displayName()));
            
            if (card) {
                card->setVerificationStatus(VerificationStatus::Verified);
                card->setProgressVisible(false);
                card->flash(FSColor(Verified));
            }
            
            m_trayIcon->notifyVerificationResult(deviceInfo->displayName(), VerificationStatus::Verified);
            {
                VerifyHistoryEntry he;
                he.deviceNode = deviceNode;
                he.deviceLabel = deviceInfo->displayName();
                he.mountPoint = deviceInfo->mountPoint;
                he.kind = VerifyHistoryKind::Hash;
                he.status = QStringLiteral("pass");
                he.summary = QStringLiteral("Hash matches (%1)").arg(result.algorithm);
                he.durationMs = result.durationMs;
                recordVerifyHistory(he);
            }
            finishVerified();
        } else {
            logMessage(QString("ALERT: %1 - hash MISMATCH!").arg(deviceInfo->displayName()), LogLevel::Security);
            m_lastVerificationHashes[deviceNode] = result.hash;

            if (card) {
                card->setVerificationStatus(VerificationStatus::Modified);
                card->setProgressVisible(false);
            }

            m_trayIcon->notifyVerificationResult(deviceInfo->displayName(), VerificationStatus::Modified);
            {
                VerifyHistoryEntry he;
                he.deviceNode = deviceNode;
                he.deviceLabel = deviceInfo->displayName();
                he.mountPoint = deviceInfo->mountPoint;
                he.kind = VerifyHistoryKind::Hash;
                he.status = QStringLiteral("mismatch");
                he.summary = QStringLiteral("Hash mismatch");
                he.durationMs = result.durationMs;
                recordVerifyHistory(he);
            }

            const bool offerMount = !deviceInfo->isMounted;
            if (m_settings.requireConfirmationForModified) {
                showModifiedDeviceAlert(*deviceInfo, record->hash, result.hash, offerMount);
            } else if (pending == PendingHashAction::MountAfterVerify
                       && !m_settings.blockModifiedDevices) {
                acceptFingerprintAndMount(*deviceInfo, result.hash, result.algorithm);
            }
        }
    } else {
        m_database->updateHash(deviceId, result.hash, result.algorithm, result.durationMs,
                               result.hashScopeLabel, result.scanModeLabel);
        logMessage(QString("Hash stored for %1").arg(deviceInfo->displayName()));
        {
            VerifyHistoryEntry he;
            he.deviceNode = deviceNode;
            he.deviceLabel = deviceInfo->displayName();
            he.mountPoint = deviceInfo->mountPoint;
            he.kind = VerifyHistoryKind::Hash;
            he.status = QStringLiteral("pass");
            he.summary = QStringLiteral("Baseline stored (%1)").arg(result.algorithm);
            he.durationMs = result.durationMs;
            recordVerifyHistory(he);
        }
        
        if (card) {
            card->setVerificationStatus(VerificationStatus::Verified);
            card->setProgressVisible(false);
        }
        
        m_trayIcon->notifyHashCompleted(deviceInfo->displayName(), result.durationMs, result.speedMBps());
        finishVerified();
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
    const PendingHashAction pending = m_pendingHashActions.take(deviceNode);

    logMessage(QString("Hash failed for %1: %2").arg(deviceNode, error), LogLevel::Error);
    {
        VerifyHistoryEntry he;
        he.deviceNode = deviceNode;
        auto info = m_deviceMonitor->getDevice(deviceNode);
        he.deviceLabel = info ? info->displayName() : deviceNode;
        he.kind = VerifyHistoryKind::Hash;
        he.status = QStringLiteral("error");
        he.summary = error;
        recordVerifyHistory(he);
    }

    if (pending == PendingHashAction::UnmountAfterVerify) {
        offerUnmountWithoutHash(deviceNode, error);
    }
    
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
    m_pendingHashActions.remove(deviceNode);
    
    logMessage(QString("Hash cancelled for %1").arg(deviceNode));
    
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setProgressVisible(false);
        const VerificationStatus prior = m_preHashStatus.take(deviceNode);
        if (prior != VerificationStatus::Unknown) {
            card->setVerificationStatus(prior);
        } else {
            card->setVerificationStatus(VerificationStatus::Pending);
        }
    }
    m_hashJobContext.remove(jobId);

    if (m_activeHashCount == 0) {
        m_trayIcon->setHashingActive(false);
        m_hashStatusLabel->clear();
    }

    updateSidebarStats();
}

// ============================================================================
// Mount Events
// ============================================================================

void MainWindow::triggerIsoVerificationOnMount(const MountManager::MountResult& result)
{
    if (!m_isoWidget || result.mountPoint.isEmpty()) return;
    if (!m_settings.isoAutoVerifyOnUsbMount && m_settings.appModule != AppModule::IsoVerifier) return;
    if (m_isoVerifyTriggeredMounts.contains(result.mountPoint)) {
        return;
    }
    m_isoVerifyTriggeredMounts.insert(result.mountPoint);
    QString label = result.deviceNode;
    if (auto info = m_deviceMonitor->getDevice(result.deviceNode)) {
        label = info->displayName();
    }
    logMessage(QStringLiteral("Auto ISO verification on %1 at %2").arg(label, result.mountPoint));
    m_isoWidget->verifyMountPoint(result.mountPoint, result.deviceNode, label);
    if (m_settings.appModule == AppModule::IsoVerifier) {
        showAndRaise();
    }
}

void MainWindow::onMountCompleted(const MountManager::MountResult& result)
{
    if (result.success) {
        logMessage(QString("Mounted %1 at %2").arg(result.deviceNode, result.mountPoint));
        m_deviceMonitor->rescan();
        if (m_pendingHashActions.value(result.deviceNode) == PendingHashAction::MountAfterVerify) {
            m_pendingHashActions.remove(result.deviceNode);
            startDeviceVerification(result.deviceNode);
        }
        triggerIsoVerificationOnMount(result);
    } else {
        logMessage(QString("Mount failed for %1: %2").arg(result.deviceNode, result.errorMessage), LogLevel::Error);
    }
}

void MainWindow::onUnmountCompleted(const MountManager::UnmountResult& result)
{
    if (m_unmountBeforeHash.remove(result.deviceNode)) {
        if (result.success) {
            logMessage(QString("Unmounted %1; starting hash").arg(result.deviceNode));
            m_deviceMonitor->rescan();
            if (m_pendingHashLaunch.contains(result.deviceNode)) {
                const PendingHashLaunch pending = m_pendingHashLaunch.take(result.deviceNode);
                startHashJob(result.deviceNode, pending.hashDeviceNode, pending.scope,
                             pending.mode, pending.resume);
            } else {
                startHashing(result.deviceNode, true);
            }
        } else {
            logMessage(QString("Pre-hash unmount failed for %1: %2")
                           .arg(result.deviceNode, result.errorMessage),
                       LogLevel::Error);
            m_pendingHashActions.remove(result.deviceNode);
        }
        return;
    }

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

    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo) {
        return;
    }

    DeviceCard* card = getDeviceCard(deviceNode);
    if (card && card->verificationStatus() == VerificationStatus::Verified) {
        m_mountManager->mount(deviceNode);
        return;
    }

    if (card && card->verificationStatus() == VerificationStatus::Modified) {
        const QString actual = m_lastVerificationHashes.value(deviceNode);
        if (!actual.isEmpty()) {
            auto record = m_database->getDevice(canonicalDeviceId(*deviceInfo));
            const QString expected = record ? record->hash : QString();
            showModifiedDeviceAlert(*deviceInfo, expected, actual, !deviceInfo->isMounted);
            return;
        }
    }

    m_pendingHashActions[deviceNode] = PendingHashAction::MountAfterVerify;
    startHashing(deviceNode);
}

void MainWindow::onUnmountRequested(const QString& deviceNode)
{
    auto device = m_deviceMonitor->getDevice(deviceNode);
    if (!device) return;
    
    if (m_settings.autoHashOnEject) {
        logMessage(QString("Re-hashing before unmount: %1").arg(deviceNode));
        m_pendingHashActions[deviceNode] = PendingHashAction::UnmountAfterVerify;
        startHashing(deviceNode);
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
    m_historyFilterDevice = deviceNode;
    refreshVerifyHistoryPanel(deviceNode);

    auto info = m_deviceMonitor->getDevice(deviceNode);
    if (!info) {
        return;
    }

    if (!info->mountPoint.isEmpty() && info->isMounted) {
        if (m_modeTabBar) {
            m_modeTabBar->setCurrentIndex(1);
            onModeTabChanged(1);
        }
        if (m_isoWidget) {
            m_isoWidget->focusDevice(deviceNode, info->mountPoint, info->displayName());
        }
        logMessage(QStringLiteral("ISO verify: %1 (%2)").arg(info->displayName(), info->mountPoint),
                   LogLevel::Info);
        showAndRaise();
        return;
    }

    logMessage(QStringLiteral("Verify history filtered to %1").arg(info->displayName()),
               LogLevel::Info);
}


void MainWindow::recordVerifyHistory(const VerifyHistoryEntry& entry)
{
    VerifyHistory::instance().append(entry);
    refreshVerifyHistoryPanel(m_historyFilterDevice);
}

void MainWindow::refreshVerifyHistoryPanel(const QString& deviceNodeFilter)
{
    if (!m_historyList) {
        return;
    }
    m_historyList->clear();

    const QList<VerifyHistoryEntry> entries =
        deviceNodeFilter.isEmpty()
            ? VerifyHistory::instance().recentEntries(30)
            : VerifyHistory::instance().entriesForDevice(deviceNodeFilter, 20);

    if (m_historyFilterLabel) {
        if (deviceNodeFilter.isEmpty()) {
            m_historyFilterLabel->setText(QStringLiteral("Verify history"));
        } else {
            auto info = m_deviceMonitor->getDevice(deviceNodeFilter);
            const QString name = info ? info->displayName() : deviceNodeFilter;
            m_historyFilterLabel->setText(QStringLiteral("Verify history — %1").arg(name));
        }
    }

    for (const VerifyHistoryEntry& e : entries) {
        auto* item = new QListWidgetItem(VerifyHistory::instance().formatEntryLine(e));
        item->setData(Qt::UserRole, e.deviceNode);
        if (e.status == QStringLiteral("pass")) {
            item->setForeground(FSColor(Verified));
        } else if (e.status == QStringLiteral("mismatch") || e.status == QStringLiteral("fail")) {
            item->setForeground(FSColor(Error));
        }
        m_historyList->addItem(item);
    }
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
    connect(&dialog, &SettingsDialog::exportDatabaseRequested, this,
            [this](const QString& path) {
                if (m_database->exportToFile(path)) {
                    logMessage(QString("Database exported to %1").arg(path));
                    QMessageBox::information(this, "Export Complete",
                                             "Database exported successfully.");
                } else {
                    QMessageBox::warning(this, "Export Failed",
                                         "Could not export the database.");
                }
            });
    connect(&dialog, &SettingsDialog::importDatabaseRequested, this,
            [this](const QString& path, bool merge) {
                const int count = m_database->importFromFile(path, merge);
                if (count >= 0) {
                    logMessage(QString("Imported %1 device(s) from %2").arg(count).arg(path));
                    updateSidebarStats();
                    QMessageBox::information(this, "Import Complete",
                                             QString("Imported %1 device(s).").arg(count));
                } else {
                    QMessageBox::warning(this, "Import Failed",
                                         "Could not import the database file.");
                }
            });
    connect(&dialog, &SettingsDialog::backupDatabaseRequested, this, [this, &dialog]() {
        const QString backupPath = m_database->createBackup();
        if (!backupPath.isEmpty()) {
            logMessage(QString("Database backup: %1").arg(backupPath));
            QMessageBox::information(&dialog, "Backup Created",
                                     QString("Backup saved to:\n%1").arg(backupPath));
        } else {
            QMessageBox::warning(&dialog, "Backup Failed",
                                 "Could not create a database backup.");
        }
    });
    connect(&dialog, &SettingsDialog::clearDatabaseRequested, this, [this]() {
        m_database->clearAllDevices();
        logMessage("Database cleared", LogLevel::Warning);
        updateSidebarStats();
    });
    
    if (dialog.exec() == QDialog::Accepted) {
        const QString previousDbPath = m_database->databasePath();
        m_settings = dialog.getSettings();
        applySettings(m_settings);
        saveSettings();

        if (!m_settings.databasePath.isEmpty()
            && m_settings.databasePath != previousDbPath) {
            m_database->initialize(m_settings.databasePath);
        }
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
    applyAppModule();
    if (m_isoWidget) {
        m_isoWidget->setActiveProfile(settings.settingsProfile);
    }
    configureBadUsbMonitoring();

    if (AutostartManager::isAvailable()) {
        const auto current = AutostartManager::isLoginAutostartEnabled();
        if (!current.has_value() || *current != settings.autoStartAtLogin) {
            QString error;
            if (!AutostartManager::setLoginAutostartEnabled(settings.autoStartAtLogin, &error)) {
                logMessage(QStringLiteral("Autostart: %1").arg(error), LogLevel::Warning);
                QMessageBox::warning(this, QStringLiteral("Autostart"),
                                     QStringLiteral("Could not update login autostart:\n%1").arg(error));
            } else {
                const QString mode = settings.autoStartAtLogin
                    ? QStringLiteral("enabled")
                    : QStringLiteral("disabled");
                logMessage(QStringLiteral("Login autostart %1 (%2)")
                               .arg(mode, AutostartManager::backendDescription()),
                           LogLevel::Info);
            }
        }
    }
}

void MainWindow::updateStatusBar()
{
    int connected = m_deviceCards.size();
    int whitelisted = m_database->deviceCount();

    m_statusLabel->setText(QString("Connected: %1 | Whitelisted: %2")
                             .arg(connected)
                             .arg(whitelisted));

    if (!m_catalogStatusBtn) {
        return;
    }
    IsoCatalogManifest::ensureLoaded();
    const QString catalogDetail = IsoCatalogManifest::integrityStatusText();
    m_catalogStatusBtn->setToolTip(
        catalogDetail + QStringLiteral("\n\nClick to open ISO verify."));
    if (!IsoCatalogManifest::lastEmbeddedIntegrityOk()) {
        m_catalogStatusBtn->setText(QStringLiteral("⚠ ISO catalog"));
        m_catalogStatusBtn->setStyleSheet(QString("color: %1; font-weight: 600; text-align: left;")
                                              .arg(FSStyle.colorCss(StyleManager::ColorRole::Warning)));
    } else {
        m_catalogStatusBtn->setText(
            QStringLiteral("Catalog: %1").arg(IsoCatalogManifest::entryCount()));
        m_catalogStatusBtn->setStyleSheet(QString("color: %1; text-align: left;")
                                              .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    }
    refreshDeviceTable();
}

void MainWindow::syncModeTabFromSettings()
{
    if (!m_modeTabBar) {
        return;
    }
    int index = 0;
    if (m_settings.appModule == AppModule::IsoVerifier) {
        index = 1;
    } else if (m_settings.appModule == AppModule::BadUsbMonitor) {
        index = 2;
    }
    if (m_appModeStack && m_appModeStack->currentIndex() > 2) {
        index = m_appModeStack->currentIndex();
    }
    if (m_modeTabBar->currentIndex() != index) {
        m_modeTabBar->blockSignals(true);
        m_modeTabBar->setCurrentIndex(index);
        m_modeTabBar->blockSignals(false);
    }
    if (m_searchEdit) {
        m_searchEdit->setVisible(index == 0);
    }
    updateNavigationState(index);
}

void MainWindow::onModeTabChanged(int index)
{
    if (index == 1) {
        m_settings.appModule = AppModule::IsoVerifier;
    } else if (index == 2) {
        m_settings.appModule = AppModule::BadUsbMonitor;
    } else if (index > 2) {
        if (m_appModeStack) {
            m_appModeStack->setCurrentIndex(index);
        }
        if (m_searchEdit) {
            m_searchEdit->setVisible(false);
        }
        updateNavigationState(index);
        return;
    } else {
        m_settings.appModule = AppModule::UsbMonitor;
    }
    if (m_searchEdit) {
        m_searchEdit->setVisible(index == 0);
    }
    updateNavigationState(index);
    applyAppModule();
    saveSettings();
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
    connect(card, &DeviceCard::cancelHashRequested, this, [this](const QString& node) {
        for (auto it = m_hashJobDevices.constBegin(); it != m_hashJobDevices.constEnd(); ++it) {
            if (it.value() == node) {
                m_hashWorker->cancelHash(it.key());
                break;
            }
        }
    });
    connect(card, &DeviceCard::watchListRequested, this, &MainWindow::onWatchListRequested);
    connect(card, &DeviceCard::acceptFingerprintRequested,
            this, &MainWindow::onAcceptFingerprintRequested);
    connect(card, &DeviceCard::openMountPointRequested, this, &MainWindow::onOpenMountPointRequested);
    connect(card, &DeviceCard::clicked, this, &MainWindow::onDeviceCardClicked);
    
    // Insert before the stretch
    m_deviceListLayout->insertWidget(m_deviceListLayout->count() - 1, card);
    m_deviceCards[device.deviceNode] = card;
    m_deviceConnectedAt.insert(device.deviceNode, QDateTime::currentDateTime());
    card->hide(); // Dashboard table is the primary UI; cards remain as state/action adapters.
    refreshDeviceTable();
    
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
        m_deviceConnectedAt.remove(deviceNode);
        card->deleteLater();
    }
    refreshDeviceTable();
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
    refreshDeviceTable();
}



int MainWindow::partitionCountFor(const DeviceInfo& device) const
{
    const QString drive = driveKey(device);
    int count = 0;
    for (const DeviceInfo& part : m_deviceMonitor->connectedDevices()) {
        if (driveKey(part) == drive) {
            ++count;
        }
    }
    return qMax(1, count);
}

QString MainWindow::resolveHashDeviceNode(const DeviceInfo& device, HashScope scope) const
{
    if (scope == HashScope::WholeDisk && !device.parentDevice.isEmpty()) {
        return device.parentDevice;
    }
    return device.deviceNode;
}

QString MainWindow::hashStorageIdFor(const DeviceInfo& device, HashScope scope) const
{
    if (scope == HashScope::WholeDisk) {
        return device.uniqueId() + QStringLiteral("_WHOLEDISK");
    }
    return m_database->canonicalUniqueId(device);
}

void MainWindow::promptAndStartHash(const QString& deviceNode, bool allowDialog)
{
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo) {
        return;
    }

    HashScope scope = m_settings.defaultHashScope;
    HashScanMode mode = m_settings.defaultHashScanMode;
    bool resume = false;

    const int parts = partitionCountFor(*deviceInfo);
    const QString deviceId = m_database->canonicalUniqueId(*deviceInfo);
    auto record = m_database->getDevice(deviceId);
    const bool hasBaseline = record && record->watchManifest.hasBaseline();

    const QString algo = record && !record->hashAlgorithm.isEmpty() ? record->hashAlgorithm
                                                                      : m_settings.hashAlgorithm;
    const QString hashNodePreview = resolveHashDeviceNode(*deviceInfo, scope);
    const bool hasCheckpoint =
        m_settings.hashResumeCheckpoints
        && HashCheckpointStore::instance()
               .checkpointFor(hashNodePreview, algo, hashScanModeToString(mode))
               .has_value();

    const bool needDialog = allowDialog && m_settings.promptHashOptionsOnManual
        && (parts > 1 || hasBaseline || hasCheckpoint);

    if (needDialog) {
        HashOptionsDialog dlg(*deviceInfo, parts, hasBaseline, hasCheckpoint, scope, mode, this);
        if (dlg.exec() != QDialog::Accepted || !dlg.choice().accepted) {
            return;
        }
        scope = dlg.choice().scope;
        mode = dlg.choice().scanMode;
        resume = dlg.choice().resumeFromCheckpoint;
    } else if (hasCheckpoint && m_settings.hashResumeCheckpoints && mode == HashScanMode::Full) {
        resume = true;
    }

    if (mode == HashScanMode::WatchManifestOnly) {
        startManifestVerification(deviceNode);
        return;
    }

    const QString hashNode = resolveHashDeviceNode(*deviceInfo, scope);
    startHashJob(deviceNode, hashNode, scope, mode, resume);
}

void MainWindow::startHashJob(const QString& uiDeviceNode, const QString& hashDeviceNode,
                              HashScope scope, HashScanMode mode, bool resume)
{
    auto deviceInfo = m_deviceMonitor->getDevice(uiDeviceNode);
    if (!deviceInfo) {
        return;
    }

    DeviceCard* card = getDeviceCard(uiDeviceNode);
    if (card) {
        m_preHashStatus[uiDeviceNode] = card->verificationStatus();
    }

    const QString storageId = hashStorageIdFor(*deviceInfo, scope);

    if (!m_unmountBeforeHash.contains(uiDeviceNode)) {
        const bool mounted = deviceInfo->isMounted
            || !m_mountManager->getMountPoint(uiDeviceNode).isEmpty();
        if (mounted && hashDeviceNode == uiDeviceNode) {
            logMessage(QString("Unmounting %1 before hash verification").arg(uiDeviceNode));
            m_unmountBeforeHash.insert(uiDeviceNode);
            m_pendingHashLaunch[uiDeviceNode] = {hashDeviceNode, scope, mode, resume};
            m_mountManager->unmount(uiDeviceNode);
            return;
        }
    }

    HashWorker::HashJob job;
    job.deviceNode = hashDeviceNode;
    job.scope = scope;
    job.scanMode = mode;
    job.resumeFromCheckpoint = resume;
    job.canonicalStorageId = storageId;
    job.bufferSizeKB = m_settings.hashBufferSizeKB;
    job.useMemoryMapping = m_settings.useMemoryMapping && mode == HashScanMode::Full && !resume;

    if (auto record = m_database->getDevice(storageId)) {
        job.algorithm = HashWorker::algorithmFromName(
            record->hashAlgorithm.isEmpty() ? m_settings.hashAlgorithm : record->hashAlgorithm);
    } else {
        job.algorithm = HashWorker::algorithmFromName(m_settings.hashAlgorithm);
    }

    const QString jobId = m_hashWorker->startHash(job);
    m_hashJobDevices[jobId] = uiDeviceNode;
    HashJobContext ctx;
    ctx.uiDeviceNode = uiDeviceNode;
    ctx.storageId = storageId;
    ctx.scope = scope;
    ctx.scanMode = mode;
    m_hashJobContext[jobId] = ctx;
}


void MainWindow::startHashing(const QString& deviceNode, bool skipUnmount)
{
    if (m_hashJobDevices.values().contains(deviceNode)) {
        return;
    }
    if (skipUnmount) {
        if (m_pendingHashLaunch.contains(deviceNode)) {
            const PendingHashLaunch pending = m_pendingHashLaunch.take(deviceNode);
            startHashJob(deviceNode, pending.hashDeviceNode, pending.scope, pending.mode,
                         pending.resume);
            return;
        }
    }
    promptAndStartHash(deviceNode, false);
}

QString MainWindow::canonicalDeviceId(const DeviceInfo& device)
{
    return m_database->canonicalUniqueId(device);
}

void MainWindow::hashAllPartitionsOnParent(const DeviceInfo& device)
{
    if (!m_settings.autoHashOnConnect || device.parentDevice.isEmpty()) {
        return;
    }

    for (const DeviceInfo& sibling : m_deviceMonitor->connectedDevices()) {
        if (sibling.parentDevice != device.parentDevice) {
            continue;
        }
        if (sibling.deviceNode == device.deviceNode) {
            continue;
        }
        if (!m_database->hasDevice(sibling)) {
            continue;
        }
        if (m_hashJobDevices.values().contains(sibling.deviceNode)) {
            continue;
        }
        startDeviceVerification(sibling.deviceNode);
    }
}

void MainWindow::mountIfVerified(const QString& deviceNode)
{
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo || deviceInfo->isMounted) {
        return;
    }

    auto record = m_database->getDevice(canonicalDeviceId(*deviceInfo));
    if (!record) {
        return;
    }

    if (record->autoMount || record->trustLevel >= 1) {
        m_mountManager->mount(deviceNode);
    }
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
    
    if (m_logList) {
        QListWidgetItem* item = new QListWidgetItem(logText);
        item->setForeground(QColor(color));
        m_logList->addItem(item);
        m_logList->scrollToBottom();

        // Keep log size reasonable
        while (m_logList->count() > 500) {
            delete m_logList->takeItem(0);
        }
    }
    appendEventRow(prefix, QString(), QString(), prefix, message, level);
    
    qDebug() << QString("[%1] %2").arg(prefix, message);
}

void MainWindow::updateEmptyState()
{
    if (!m_contentStack || !m_emptyStateLabel) {
        return;
    }
    if (m_deviceCards.isEmpty()) {
        m_emptyStateLabel->setText(
            "💾\n\nNo USB devices connected\n\n"
            "Connect a USB flash drive or switch to ISO Verify in Settings.\n\n"
            "Recommended: watch selected folders (fast) or automatic ISO checks — "
            "full-partition hashing is optional for advanced users."
        );
        m_contentStack->setCurrentIndex(1);  // Empty state
    } else {
        m_contentStack->setCurrentIndex(0);  // Device list
    }
}

void MainWindow::updateSidebarStats()
{
    if (m_connectedCountLabel) {
        m_connectedCountLabel->setText(QString::number(m_deviceCards.size()));
    }
    if (m_whitelistedCountLabel) {
        m_whitelistedCountLabel->setText(QString::number(m_database ? m_database->deviceCount() : 0));
    }
    if (m_hashingCountLabel) {
        m_hashingCountLabel->setText(QString::number(m_activeHashCount));
    }
    if (m_blockedCountLabel) {
        m_blockedCountLabel->setText(QString::number(m_rejectedDrives.size()));
    }
    if (m_totalEventsLabel) {
        m_totalEventsLabel->setText(QString::number(m_totalEventCount));
    }
    
    m_trayIcon->setDeviceCount(m_deviceCards.size(), m_database->deviceCount());
    refreshDeviceTable();
}

void MainWindow::refreshDeviceTable()
{
    if (!m_deviceTable) {
        return;
    }
    m_deviceTable->setRowCount(0);
    auto addActionButton = [this](int row, const QString& key, bool hid) {
        auto* button = new QToolButton;
        button->setText(QStringLiteral("Actions"));
        button->setPopupMode(QToolButton::InstantPopup);
        button->setObjectName(QStringLiteral("DashboardActionButton"));
        auto* menu = new QMenu(button);
        if (hid) {
            QAction* trust = menu->addAction(QStringLiteral("Trust / baseline"));
            connect(trust, &QAction::triggered, this, [this, key]() { onBadUsbTrustRequested(key); });
            QAction* capture = menu->addAction(QStringLiteral("Capture USB traffic"));
            connect(capture, &QAction::triggered, this, [this, key]() { onBadUsbCaptureRequested(key); });
        } else {
            QAction* mount = menu->addAction(QStringLiteral("Mount"));
            connect(mount, &QAction::triggered, this, [this, key]() { onMountRequested(key); });
            QAction* unmount = menu->addAction(QStringLiteral("Unmount"));
            connect(unmount, &QAction::triggered, this, [this, key]() { onUnmountRequested(key); });
            QAction* verify = menu->addAction(QStringLiteral("Verify / hash"));
            connect(verify, &QAction::triggered, this, [this, key]() { onRehashRequested(key); });
            QAction* watch = menu->addAction(QStringLiteral("Watch lists"));
            connect(watch, &QAction::triggered, this, [this, key]() { onWatchListRequested(key); });
        }
        button->setMenu(menu);
        m_deviceTable->setCellWidget(row, 6, button);
    };

    const QList<DeviceCard*> cards = m_deviceCards.values();
    for (DeviceCard* card : cards) {
        const DeviceInfo device = card->device();
        const int row = m_deviceTable->rowCount();
        m_deviceTable->insertRow(row);

        const QString type = device.fsType.isEmpty()
            ? QStringLiteral("USB Device")
            : QStringLiteral("Mass Storage");
        const QString status = verificationStatusToString(card->verificationStatus());
        const QString capacity = device.sizeBytes > 0
            ? QStringLiteral("%1 GB").arg(static_cast<double>(device.sizeBytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1)
            : QStringLiteral("—");
        const QString vendorModel = QStringList{device.vendor, device.model}
            .filter(QRegularExpression(QStringLiteral(".+"))).join(QStringLiteral("\n"));
        const QString connectedAt = m_deviceConnectedAt.value(device.deviceNode, QDateTime::currentDateTime())
                                        .toString(QStringLiteral("M/d/yyyy h:mm:ss AP"));

        auto setItem = [this, row](int column, const QString& text, const QColor& foreground = QColor()) {
            auto* item = new QTableWidgetItem(text);
            item->setToolTip(text);
            if (foreground.isValid()) {
                item->setForeground(foreground);
            }
            m_deviceTable->setItem(row, column, item);
        };
        setItem(0, device.displayName() + QStringLiteral("\n") + device.deviceNode);
        setItem(1, type);
        QColor statusColor = FSStyle.color(StyleManager::ColorRole::TextSecondary);
        if (card->verificationStatus() == VerificationStatus::Verified) {
            statusColor = FSStyle.color(StyleManager::ColorRole::Success);
        } else if (card->verificationStatus() == VerificationStatus::Modified
                   || card->verificationStatus() == VerificationStatus::Error) {
            statusColor = FSStyle.color(StyleManager::ColorRole::Error);
        } else if (card->verificationStatus() == VerificationStatus::NewDevice) {
            statusColor = FSStyle.color(StyleManager::ColorRole::Warning);
        }
        setItem(2, status, statusColor);
        setItem(3, capacity);
        setItem(4, vendorModel.isEmpty() ? QStringLiteral("—") : vendorModel);
        setItem(5, connectedAt);
        addActionButton(row, device.deviceNode, false);
    }

    for (const HidDeviceInfo& hid : m_hidDashboardDevices) {
        const int row = m_deviceTable->rowCount();
        m_deviceTable->insertRow(row);
        const QString stableId = hid.stableId();
        const auto baseline = m_badUsbBaselineStore ? m_badUsbBaselineStore->matchDevice(hid) : std::nullopt;
        const bool trusted = baseline.has_value() && baseline->trusted;
        auto setItem = [this, row](int column, const QString& text, const QColor& foreground = QColor()) {
            auto* item = new QTableWidgetItem(text);
            item->setToolTip(text);
            if (foreground.isValid()) {
                item->setForeground(foreground);
            }
            m_deviceTable->setItem(row, column, item);
        };
        setItem(0, hid.displayName() + QStringLiteral("\n") + stableId);
        setItem(1, QStringLiteral("HID Device"));
        setItem(2, trusted ? QStringLiteral("Allowed") : QStringLiteral("Learning"),
                trusted ? FSStyle.color(StyleManager::ColorRole::Success)
                        : FSStyle.color(StyleManager::ColorRole::Warning));
        setItem(3, QStringLiteral("—"));
        setItem(4, QStringList{hid.manufacturer, hid.product}.filter(QRegularExpression(QStringLiteral(".+"))).join(QStringLiteral("\n")));
        setItem(5, hid.seenAtUtc.isValid()
                       ? hid.seenAtUtc.toLocalTime().toString(QStringLiteral("M/d/yyyy h:mm:ss AP"))
                       : QStringLiteral("—"));
        addActionButton(row, stableId, true);
    }
    m_deviceTable->resizeRowsToContents();
}

void MainWindow::appendEventRow(const QString& event, const QString& device, const QString& type,
                                const QString& result, const QString& details, LogLevel level)
{
    if (!m_eventTable) {
        return;
    }
    ++m_totalEventCount;
    if (m_totalEventsLabel) {
        m_totalEventsLabel->setText(QString::number(m_totalEventCount));
    }
    const int row = 0;
    m_eventTable->insertRow(row);
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("M/d/yyyy h:mm:ss AP"));
    const QStringList values = {
        time,
        event,
        device,
        type,
        result,
        details,
    };
    QColor color = FSStyle.color(StyleManager::ColorRole::TextSecondary);
    if (level == LogLevel::Security || level == LogLevel::Error) {
        color = FSStyle.color(StyleManager::ColorRole::Error);
    } else if (level == LogLevel::Warning) {
        color = FSStyle.color(StyleManager::ColorRole::Warning);
    } else if (level == LogLevel::Info) {
        color = FSStyle.color(StyleManager::ColorRole::AccentPrimary);
    }
    for (int column = 0; column < values.size(); ++column) {
        auto* item = new QTableWidgetItem(values.at(column));
        if (column == 1 || column == 4) {
            item->setForeground(color);
        }
        item->setToolTip(values.at(column));
        m_eventTable->setItem(row, column, item);
    }
    while (m_eventTable->rowCount() > 100) {
        m_eventTable->removeRow(m_eventTable->rowCount() - 1);
    }
}

void MainWindow::updateNavigationState(int index)
{
    for (int i = 0; i < m_navButtons.size(); ++i) {
        bool active = false;
        if (index == 0 && i == 0) active = true;
        if (index == 3 && i == 1) active = true;
        if (index == 4 && i == 2) active = true;
        if ((index == 2 || index == 5) && i == 3) active = true;
        if (index == 1 && i == 4) active = true;
        if (index == 6 && i == 6) active = true;
        m_navButtons.at(i)->setChecked(active);
    }
}

bool MainWindow::showNewDriveDialog(const DeviceInfo& device)
{
    const QString drive = driveKey(device);
    int partitionCount = 0;
    QStringList partitionNodes;

    for (const DeviceInfo& part : m_deviceMonitor->connectedDevices()) {
        if (driveKey(part) != drive) {
            continue;
        }
        ++partitionCount;
        partitionNodes.append(part.deviceNode);
    }

    QString message = QString(
        "<b>Unknown USB drive detected:</b><br><br>"
        "<b>Drive:</b> %1<br>"
        "<b>Representative partition:</b> %2<br>"
        "<b>Serial:</b> %3<br>"
        "<b>Partitions detected:</b> %4<br>"
        "<b>Partition nodes:</b> %5<br><br>"
        "Add this <b>entire drive</b> to the whitelist and hash all partitions?")
        .arg(drive)
        .arg(device.deviceNode)
        .arg(device.serial.isEmpty() ? "N/A" : device.serial)
        .arg(partitionCount)
        .arg(partitionNodes.join(", "));

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("New Drive Detected");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    return msgBox.exec() == QMessageBox::Yes;
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

void MainWindow::acceptFingerprint(const DeviceInfo& device, const QString& actualHash,
                                   const QString& algorithm, bool mountAfter)
{
    const QString deviceId = canonicalDeviceId(device);
    m_database->updateHash(deviceId, actualHash, algorithm, 0);
    m_lastVerificationHashes.remove(device.deviceNode);

    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Verified);
        if (auto updated = m_database->getDevice(deviceId)) {
            card->setDeviceRecord(*updated);
        }
    }

    updateSidebarStats();

    if (mountAfter) {
        m_mountManager->mount(device.deviceNode);
        logMessage(QString("Fingerprint accepted and mount requested: %1")
                       .arg(device.displayName()),
                   LogLevel::Warning);
    } else {
        logMessage(QString("Fingerprint accepted for: %1").arg(device.displayName()),
                   LogLevel::Warning);
    }
}

void MainWindow::acceptFingerprintAndMount(const DeviceInfo& device, const QString& actualHash,
                                           const QString& algorithm)
{
    acceptFingerprint(device, actualHash, algorithm, true);
}

void MainWindow::onAcceptFingerprintRequested(const QString& deviceNode)
{
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo) {
        return;
    }

    const QString actual = m_lastVerificationHashes.value(deviceNode);
    if (actual.isEmpty()) {
        logMessage(QString("No verification hash for %1 — rehash first").arg(deviceNode),
                   LogLevel::Warning);
        startHashing(deviceNode);
        return;
    }

    auto record = m_database->getDevice(canonicalDeviceId(*deviceInfo));
    const QString expected = record ? record->hash : QString();

    if (m_settings.requireConfirmationForModified) {
        showModifiedDeviceAlert(*deviceInfo, expected, actual, false);
        return;
    }

    acceptFingerprint(*deviceInfo, actual, m_settings.hashAlgorithm, false);
}

void MainWindow::mountDespiteModification(const DeviceInfo& device)
{
    logMessage(QString("Mounting modified device without updating fingerprint: %1")
                   .arg(device.displayName()),
               LogLevel::Security);
    m_mountManager->mount(device.deviceNode);
}

bool MainWindow::showModifiedDeviceAlert(const DeviceInfo& device, const QString& expected,
                                         const QString& actual, bool offerMount)
{
    logMessage(QString("Modified device detected: %1").arg(device.displayName()), LogLevel::Security);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("Security Alert — Device May Be Tampered"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setTextFormat(Qt::RichText);

    QString body = QString(
        "<b style='color: red;'>%1 may have been tampered with.</b><br>"
        "Its contents do not match the trusted fingerprint on file.<br><br>"
        "<b>Partition:</b> %2<br>"
        "<b>Expected:</b><br><code style='font-size:10px;'>%3</code><br>"
        "<b>Current:</b><br><code style='font-size:10px;'>%4</code><br><br>")
                         .arg(device.displayName(), device.deviceNode, expected, actual);

    body += QStringLiteral(
        "<b>Approve fingerprint</b> — save the new hash as trusted; do not mount.<br>"
        "<b>Mount anyway</b> — mount now without changing the whitelist (device stays flagged).<br>");

    if (offerMount && !device.isMounted) {
        body += QStringLiteral(
            "<b>Approve and mount</b> — save the new hash and mount in one step.<br>");
    }

    if (m_settings.blockModifiedDevices) {
        body += QStringLiteral(
            "<br><i>Automatic mounting of modified devices is disabled in settings.</i>");
    }

    msgBox.setText(body);

    QPushButton* approveBtn = msgBox.addButton(QStringLiteral("Approve fingerprint"),
                                               QMessageBox::AcceptRole);
    QPushButton* mountAnywayBtn = nullptr;
    QPushButton* approveMountBtn = nullptr;

    if (offerMount && !device.isMounted) {
        mountAnywayBtn = msgBox.addButton(QStringLiteral("Mount anyway"),
                                          QMessageBox::DestructiveRole);
        approveMountBtn = msgBox.addButton(QStringLiteral("Approve and mount"),
                                           QMessageBox::ActionRole);
    }

    msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    msgBox.exec();

    if (msgBox.clickedButton() == approveBtn) {
        acceptFingerprint(device, actual, m_settings.hashAlgorithm, false);
        return true;
    }
    if (mountAnywayBtn && msgBox.clickedButton() == mountAnywayBtn) {
        mountDespiteModification(device);
        return true;
    }
    if (approveMountBtn && msgBox.clickedButton() == approveMountBtn) {
        acceptFingerprintAndMount(device, actual, m_settings.hashAlgorithm);
        return true;
    }
    return false;
}

void MainWindow::offerUnmountWithoutHash(const QString& deviceNode, const QString& error)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Hash Failed Before Eject");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(QString(
        "Could not verify the device hash before unmounting:<br><br>"
        "<code>%1</code><br><br>"
        "Unmount anyway without verification?")
        .arg(error.toHtmlEscaped()));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        logMessage(QString("User overrode hash failure; unmounting %1").arg(deviceNode), LogLevel::Warning);
        m_mountManager->unmount(deviceNode);
    }
}



} // namespace FlashSentry
