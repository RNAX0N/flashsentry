#include "MainWindow.h"
#include "AppPaths.h"
#include "AppDiagnostics.h"
#include "Platform.h"
#include "WinStorage.h"
#include "UiIcons.h"
#include "StyledMessageBox.h"
#include "AutostartManager.h"
#include "AuditLog.h"
#include "IsoCatalogManifest.h"
#include "SettingsProfiles.h"
#include "WelcomeWizard.h"
#include "EventDetailDialog.h"
#include "DeviceTimelineLog.h"
#include "BlockedDriveStore.h"
#include "policy/PolicyGateway.h"
#include "policy/PolicyServiceLocator.h"
#include "ContentPageShell.h"
#include "VerifyHistory.h"
#include "HashCheckpoint.h"
#include "HashOptionsDialog.h"
#include "AlertsPage.h"
#include "ReportsPage.h"
#include "AboutPage.h"
#include "policy/PolicyPaths.h"
#include "DeviceWhitelistService.h"
#include "DeviceDriveUtil.h"
#include "DeviceTrustCoordinator.h"

#include <algorithm>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include <QApplication>
#include <QMessageBox>
#include <QSet>
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
#include <QShortcut>
#include <QCursor>
#include <QUuid>

#ifdef Q_OS_WIN
#include <dbt.h>
#include <qt_windows.h>
#endif

namespace {

FlashSpartan::UiEventEntry makeUiEvent(const QString& event, const QString& device,
                                      const QString& type, const QString& result,
                                      const QString& detail, const QString& deviceNode = {})
{
    FlashSpartan::UiEventEntry e;
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.time = QDateTime::currentDateTime();
    e.event = event;
    e.device = device;
    e.type = type;
    e.result = result;
    e.detail = detail;
    e.deviceNode = deviceNode;
    return e;
}

bool isAlertResult(const QString& result)
{
    const QString r = result.toLower();
    return r == QLatin1String("alert") || r == QLatin1String("security")
           || r == QLatin1String("warn") || r == QLatin1String("warning")
           || r == QLatin1String("error") || r == QLatin1String("fail")
           || r == QLatin1String("failed") || r == QLatin1String("mismatch")
           || r == QLatin1String("blocked") || r == QLatin1String("rejected");
}

bool isAlertUiEvent(const FlashSpartan::UiEventEntry& e)
{
    if (isAlertResult(e.result)) {
        return true;
    }
    const QString ev = e.event.toLower();
    return ev.contains(QStringLiteral("mismatch")) || ev.contains(QStringLiteral("blocked"))
           || ev.contains(QStringLiteral("anomaly")) || ev.contains(QStringLiteral("failed"))
           || ev.contains(QStringLiteral("reject")) || ev.contains(QStringLiteral("security"));
}

QList<FlashSpartan::AuditLogRow> readAuditLogTail(const QString& path, int maxLines, bool policyFormat)
{
    QList<FlashSpartan::AuditLogRow> rows;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return rows;
    }

    QStringList lines;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        lines.append(line);
        while (lines.size() > maxLines) {
            lines.removeFirst();
        }
    }

    for (int i = lines.size() - 1; i >= 0; --i) {
        const QJsonDocument doc = QJsonDocument::fromJson(lines.at(i).toUtf8());
        if (!doc.isObject()) {
            continue;
        }
        const QJsonObject o = doc.object();
        FlashSpartan::AuditLogRow row;
        row.time = QDateTime::fromString(o.value(QStringLiteral("ts")).toString(), Qt::ISODate);
        if (!row.time.isValid()) {
            row.time = QDateTime::currentDateTime();
        }
        row.source = path;

        if (policyFormat) {
            row.category = o.value(QStringLiteral("actor")).toString();
            row.event = o.value(QStringLiteral("action")).toString();
            row.detail = o.value(QStringLiteral("target")).toString();
            row.source = o.value(QStringLiteral("detail")).toString();
        } else {
            row.event = o.value(QStringLiteral("event")).toString();
            if (row.event == QLatin1String("iso_verify")) {
                row.category = QStringLiteral("ISO");
                row.detail = o.value(QStringLiteral("path")).toString();
                if (row.detail.isEmpty()) {
                    row.detail = o.value(QStringLiteral("device")).toString();
                }
            } else if (row.event == QLatin1String("badusb_anomaly")) {
                row.category = QStringLiteral("BadUSB");
                row.detail = o.value(QStringLiteral("summary")).toString();
            } else {
                row.category = QStringLiteral("System");
                row.detail = o.value(QStringLiteral("detail")).toString();
            }
        }
        rows.append(row);
    }
    return rows;
}

} // namespace

namespace FlashSpartan {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    AppPaths::migrateQSettingsFromFlashSentry();
    m_qsettings = std::make_unique<QSettings>(QStringLiteral("flashspartan"),
                                            QStringLiteral("FlashSpartan"));
    
    // Initialize the style manager
    FSStyle.initialize();
    
    // Setup UI first
    setupUi();
    setupShortcuts();
    
    // Initialize backend components
    initializeBackend();
    
    // Connect all signals
    connectSignals();
    
    // Load settings
    loadSettings();
    VerifyHistory::instance().load();
    DeviceTimelineLog::instance().load();
    BlockedDriveStore::instance().refreshFromGateway();
    HashCheckpointStore::instance().load();

    m_liveSettingsTimer = new QTimer(this);
    m_liveSettingsTimer->setSingleShot(true);
    m_liveSettingsTimer->setInterval(400);
    connect(m_liveSettingsTimer, &QTimer::timeout, this, [this]() {
        if (m_settingsPage) {
            applyLiveSettings(m_settingsPage->currentSettings());
        }
    });
    
    // Apply styling
    applyStyle();
    refreshVerifyHistoryPanel();
    
    // Setup status update timer
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(STATUS_UPDATE_INTERVAL_MS);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start();

    m_usbMonitorRefreshTimer = new QTimer(this);
    m_usbMonitorRefreshTimer->setSingleShot(true);
    m_usbMonitorRefreshTimer->setInterval(400);
    connect(m_usbMonitorRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshUsbMonitorHome);

#ifdef Q_OS_WIN
    m_winDeviceRescanTimer = new QTimer(this);
    m_winDeviceRescanTimer->setSingleShot(true);
    m_winDeviceRescanTimer->setInterval(400);
    connect(m_winDeviceRescanTimer, &QTimer::timeout, this, [this]() {
        if (m_deviceMonitor) {
            m_deviceMonitor->rescan();
        }
        if (m_mountManager) {
            m_mountManager->refreshMountStatus();
        }
        if (m_usbHostMonitor) {
            m_usbHostMonitor->rescan();
        }
        if (m_hidMonitor) {
            m_hidMonitor->rescan();
        }
        scheduleUsbMonitorRefresh();
    });
#endif
    
    // Start device monitoring
    m_deviceMonitor->startMonitoring();
    configureBadUsbMonitoring();

#ifdef Q_OS_WIN
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive) {
                    refreshUsbPcapIntegration();
                }
            });
#endif

    // Show tray icon
    if (TrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->show();
    }
    
    logMessage("FlashSpartan started", LogLevel::Info);

    IsoCatalogManifest::ensureLoaded();
    QTimer::singleShot(0, this, [this]() { warnIfCatalogIntegrityFailed(); });
}

MainWindow::~MainWindow()
{
    m_isClosing = true;

#ifdef Q_OS_WIN
    if (m_volumeDeviceNotify) {
        UnregisterDeviceNotification(reinterpret_cast<HDEVNOTIFY>(m_volumeDeviceNotify));
        m_volumeDeviceNotify = nullptr;
    }
#endif

    // Stop monitoring
    if (m_deviceMonitor) {
        m_deviceMonitor->stopMonitoring();
    }
    if (m_usbHostMonitor) {
        m_usbHostMonitor->stopMonitoring();
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
    setWindowTitle("FlashSpartan");
    setWindowIcon(QIcon(QStringLiteral(":/icons/flashspartan.svg")));
    setMinimumSize(1100, 720);

    if (auto* screen = QApplication::primaryScreen()) {
        QRect screenGeometry = screen->availableGeometry();
        int x = (screenGeometry.width() - 1200) / 2;
        int y = (screenGeometry.height() - 760) / 2;
        setGeometry(x, y, 1200, 760);
    }

    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    auto* shell = new QHBoxLayout;
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    m_navSidebar = new NavSidebar;
    connect(m_navSidebar, &NavSidebar::pageSelected, this, &MainWindow::onNavPageSelected);
    shell->addWidget(m_navSidebar);

    m_pageStack = new QStackedWidget;
    m_pageStack->setObjectName(QStringLiteral("AppPageStack"));

    m_usbMonitorPage = new UsbMonitorPage;
    connect(m_usbMonitorPage, &UsbMonitorPage::deviceNameEdited, this,
            [this](const QString& node, const QString& name) {
                m_userDeviceNames.insert(node, name);
                if (auto info = m_deviceMonitor->getDevice(node)) {
                    DeviceInfo d = *info;
                    QString id = canonicalDeviceId(d);
                    if (auto rec = m_database->getDevice(id)) {
                        DeviceRecord updated = *rec;
                        updated.notes = name;
                        updated.lastKnownInfo = d;
                        m_database->updateDevice(updated);
                    }
                }
                refreshUsbMonitorHome();
            });
    connect(m_usbMonitorPage, &UsbMonitorPage::deviceActionsRequested, this,
            &MainWindow::showDeviceActionsMenu);
    connect(m_usbMonitorPage, &UsbMonitorPage::deviceHistoryRequested, this,
            &MainWindow::showDeviceHistory);
    connect(m_usbMonitorPage, &UsbMonitorPage::eventDetailsRequested, this,
            [this](const UiEventEntry& entry) {
                EventDetailDialog dlg(entry, this);
                dlg.exec();
            });
    m_pageStack->addWidget(m_usbMonitorPage);

    m_deviceHistoryPage = new DeviceHistoryPage;
    connect(m_deviceHistoryPage, &DeviceHistoryPage::deviceSelectionChanged, this,
            [this](const QString& node) {
                Q_UNUSED(node);
                refreshDeviceHistoryPage();
            });
    connect(m_deviceHistoryPage, &DeviceHistoryPage::eventDetailsRequested, this,
            [this](const UiEventEntry& entry) {
                EventDetailDialog dlg(entry, this);
                dlg.exec();
            });
    m_pageStack->addWidget(m_deviceHistoryPage);

    m_allowBlockListPage = new AllowBlockListPage;
    connect(m_allowBlockListPage, &AllowBlockListPage::filterChanged, this,
            &MainWindow::refreshAllowBlockListPage);
    connect(m_allowBlockListPage, &AllowBlockListPage::allowRequested, this,
            [this](const QString& uniqueId, const QString& driveKeyParam) {
                if (m_deviceMonitor) {
                    for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
                        if (this->driveKey(d) == driveKeyParam
                            || m_database->canonicalUniqueId(d) == uniqueId) {
                            allowDriveForDevice(d);
                            break;
                        }
                    }
                }
                if (!uniqueId.isEmpty()) {
                    m_database->setTrustLevel(uniqueId, qMax(1, m_settings.defaultTrustLevel));
                    BlockedDriveStore::instance().unblock(driveKeyParam, uniqueId);
                }
                refreshAllowBlockListPage();
                refreshUsbMonitorHome();
            });
    connect(m_allowBlockListPage, &AllowBlockListPage::blockRequested, this,
            [this](const QString& uniqueId, const QString& driveKey, const QString& label) {
                BlockedDriveStore::instance().block(driveKey, uniqueId, label);
                refreshAllowBlockListPage();
                refreshUsbMonitorHome();
            });
    connect(m_allowBlockListPage, &AllowBlockListPage::unblockRequested, this,
            [this](const QString& uniqueId, const QString& driveKey) {
                BlockedDriveStore::instance().unblock(driveKey, uniqueId);
                refreshAllowBlockListPage();
                refreshUsbMonitorHome();
            });
    connect(m_allowBlockListPage, &AllowBlockListPage::removeFromWhitelistRequested, this,
            [this](const QString& uniqueId) {
                m_database->removeDevice(uniqueId);
                refreshAllowBlockListPage();
                refreshUsbMonitorHome();
                updateSidebarStats();
            });
    connect(m_allowBlockListPage, &AllowBlockListPage::historyRequested, this,
            &MainWindow::showDeviceHistory);
    m_pageStack->addWidget(m_allowBlockListPage);

    m_alertsPage = new AlertsPage;
    connect(m_alertsPage, &AlertsPage::eventDetailsRequested, this,
            [this](const UiEventEntry& entry) {
                EventDetailDialog dlg(entry, this);
                dlg.exec();
            });
    connect(m_alertsPage, &AlertsPage::filterChanged, this, &MainWindow::refreshAlertsPage);
    m_pageStack->addWidget(m_alertsPage);

    m_reportsPage = new ReportsPage;
    connect(m_reportsPage, &ReportsPage::refreshRequested, this, &MainWindow::refreshReportsPage);
    connect(m_reportsPage, &ReportsPage::openAuditLogRequested, this, [this]() {
        const QString path = AuditLog::logPath();
        if (QFileInfo::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        } else {
            logMessage(QStringLiteral("Verification audit log not created yet: %1").arg(path),
                       LogLevel::Info);
        }
    });
    connect(m_reportsPage, &ReportsPage::openPolicyAuditRequested, this, [this]() {
        const QString path = Policy::PolicyPaths::auditLogPath();
        if (QFileInfo::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        } else {
            logMessage(QStringLiteral("Policy audit log not created yet: %1").arg(path),
                       LogLevel::Info);
        }
    });
    m_pageStack->addWidget(m_reportsPage);

    m_aboutPage = new AboutPage;
    connect(m_aboutPage, &AboutPage::openRepositoryRequested, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/RNAX0N/flashsentry")));
    });
    connect(m_aboutPage, &AboutPage::openUserGuideRequested, this, []() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/RNAX0N/flashsentry/blob/main/docs/USER_GUIDE.md")));
    });

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
    m_isoVerifierPage = new ContentPageShell(QStringLiteral("ISO Verifier"), m_isoWidget);
    m_pageStack->addWidget(m_isoVerifierPage);

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
    connect(m_badUsbWidget, &BadUsbWidget::downloadUsbPcapRequested, this, [this]() {
        if (m_usbPcapInstaller) {
            m_usbPcapInstaller->startDownloadAndInstall();
        }
    });
    connect(m_badUsbWidget, &BadUsbWidget::openUsbPcapPageRequested, this, []() {
        UsbPcapInstaller::openInstallPage();
    });

    connect(m_badUsbWidget, &BadUsbWidget::openCaptureFolderRequested, this, [this]() {
        if (m_usbmonCapture) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_usbmonCapture->outputDirectory()));
        }
    });
    m_badUsbMonitorPage = new ContentPageShell(QStringLiteral("BadUSB Monitor"), m_badUsbWidget);
    m_pageStack->addWidget(m_badUsbMonitorPage);

    m_settingsPage = new SettingsPage;
    connect(m_settingsPage, &SettingsPage::settingsApplyRequested, this,
            &MainWindow::applySettingsPage);
    connect(m_settingsPage, &SettingsPage::liveSettingsChanged, this,
            [this](const AppSettings& settings) {
                m_pendingLiveSettings = settings;
                m_liveSettingsTimer->start();
            });
    connect(m_settingsPage, &SettingsPage::themeChanged, this, &MainWindow::onThemeChanged);
    connect(m_settingsPage, &SettingsPage::exportDatabaseRequested, this,
            [this](const QString& path) {
                if (m_database->exportToFile(path)) {
                    logMessage(QString("Database exported to %1").arg(path));
                    QMessageBox::information(this, QStringLiteral("Export Complete"),
                                             QStringLiteral("Database exported successfully."));
                } else {
                    QMessageBox::warning(this, QStringLiteral("Export Failed"),
                                         QStringLiteral("Could not export the database."));
                }
            });
    connect(m_settingsPage, &SettingsPage::importDatabaseRequested, this,
            [this](const QString& path) {
                const int count = m_database->importFromFile(path, true);
                if (count >= 0) {
                    logMessage(QString("Imported %1 device(s) from %2").arg(count).arg(path));
                    updateSidebarStats();
                    QMessageBox::information(this, QStringLiteral("Import Complete"),
                                             QString::number(count) + QStringLiteral(" device(s) imported."));
                } else {
                    QMessageBox::warning(this, QStringLiteral("Import Failed"),
                                         QStringLiteral("Could not import the database file."));
                }
            });
    connect(m_settingsPage, &SettingsPage::backupDatabaseRequested, this, [this]() {
        const QString backupPath = m_database->createBackup();
        if (!backupPath.isEmpty()) {
            logMessage(QString("Database backup: %1").arg(backupPath));
            QMessageBox::information(this, QStringLiteral("Backup Created"),
                                     QStringLiteral("Backup saved to:\n%1").arg(backupPath));
        } else {
            QMessageBox::warning(this, QStringLiteral("Backup Failed"),
                                 QStringLiteral("Could not create a database backup."));
        }
    });
    connect(m_settingsPage, &SettingsPage::clearDatabaseRequested, this, [this]() {
        m_database->clearAllDevices();
        logMessage(QStringLiteral("Database cleared"), LogLevel::Warning);
        updateSidebarStats();
    });
    m_pageStack->addWidget(m_settingsPage);
    m_pageStack->addWidget(m_aboutPage);

    shell->addWidget(m_pageStack, 1);
    m_mainLayout->addLayout(shell, 1);

    m_hiddenDeviceHost = new QWidget;
    m_hiddenDeviceHost->setVisible(false);
    m_hiddenDeviceLayout = new QVBoxLayout(m_hiddenDeviceHost);
    m_hiddenDeviceLayout->setContentsMargins(0, 0, 0, 0);

    m_watchListsPanel = new WatchListsPanel;
    connect(m_watchListsPanel, &WatchListsPanel::editDeviceRequested, this,
            &MainWindow::onWatchListRequested);

    createStatusBar();
    m_navSidebar->setCurrentPage(AppPage::UsbMonitor);
    m_pageStack->setCurrentWidget(m_usbMonitorPage);
    refreshUsbMonitorHome();
}

QWidget* MainWindow::createHeader()
{
    m_headerWidget = new QWidget;
    m_headerWidget->setObjectName("HeaderWidget");
    m_headerWidget->setFixedHeight(84);
    
    QHBoxLayout* layout = new QHBoxLayout(m_headerWidget);
    layout->setContentsMargins(20, 10, 20, 10);
    layout->setSpacing(16);
    
    // Logo / title / tagline
    QHBoxLayout* titleLayout = new QHBoxLayout;
    titleLayout->setSpacing(12);
    
    QLabel* logoLabel = new QLabel;
    logoLabel->setFixedSize(40, 40);
    UiIcons::setLabelPixmap(logoLabel, ":/icons/flashspartan.svg", 36);
    auto* logoGlow = new QGraphicsDropShadowEffect(logoLabel);
    logoGlow->setBlurRadius(18);
    logoGlow->setColor(FSColor(AccentPrimary));
    logoGlow->setOffset(0, 0);
    logoLabel->setGraphicsEffect(logoGlow);
    titleLayout->addWidget(logoLabel);
    
    QVBoxLayout* titleTextLayout = new QVBoxLayout;
    titleTextLayout->setSpacing(0);
    m_titleLabel = new QLabel("FlashSpartan");
    m_titleLabel->setFont(FSFont(Heading2));
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    titleTextLayout->addWidget(m_titleLabel);
    QLabel* taglineLabel = new QLabel(QStringLiteral("USB Flash Drive Security Monitor"));
    taglineLabel->setFont(FSFont(Small));
    taglineLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::TextMuted)));
    titleTextLayout->addWidget(taglineLabel);
    titleLayout->addLayout(titleTextLayout);
    
    layout->addLayout(titleLayout);

    layout->addSpacing(12);

    m_modeTabBar = new QTabBar;
    m_modeTabBar->addTab(QStringLiteral("Connected Devices"));
    m_modeTabBar->addTab(QStringLiteral("ISO Verifier"));
    m_modeTabBar->addTab(QStringLiteral("Watch Lists"));
    m_modeTabBar->setDocumentMode(true);
    m_modeTabBar->setExpanding(false);
    m_modeTabBar->setStyleSheet(FSStyle.tabBarStyleSheet());
    connect(m_modeTabBar, &QTabBar::currentChanged, this, &MainWindow::onModeTabChanged);
    layout->addWidget(m_modeTabBar);

    layout->addStretch();

    // Search box
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Search devices..."));
    m_searchEdit->setFixedWidth(250);
    m_searchEdit->setClearButtonEnabled(true);
    UiIcons::addLeadingSearchAction(m_searchEdit);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    layout->addWidget(m_searchEdit);
    
    layout->addSpacing(16);
    
    // Refresh button
    m_refreshBtn = new QPushButton(QStringLiteral("Refresh"));
    UiIcons::setButtonIcon(m_refreshBtn, ":/icons/refresh.svg", 18);
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setToolTip("Rescan for USB devices");
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    layout->addWidget(m_refreshBtn);
    
    // Settings button
    m_settingsBtn = new QPushButton;
    m_settingsBtn->setFixedSize(40, 40);
    UiIcons::setButtonIcon(m_settingsBtn, ":/icons/settings.svg", 22);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("Settings");
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    layout->addWidget(m_settingsBtn);
    
    return m_headerWidget;
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
        QLabel* iconLabel = new QLabel;
        if (icon.startsWith(QStringLiteral(":/"))) {
            UiIcons::setLabelPixmap(iconLabel, icon.toUtf8().constData(), 20);
            iconLabel->setFixedSize(24, 24);
        } else {
            iconLabel->setText(icon);
            iconLabel->setStyleSheet("font-size: 16px;");
        }
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
    
    addStatRow(0, "Connected", m_connectedCountLabel, QStringLiteral(":/icons/usb-drive.svg"));
    addStatRow(1, "Whitelisted", m_whitelistedCountLabel, QStringLiteral(":/icons/shield-check.svg"));
    addStatRow(2, "Hashing", m_hashingCountLabel, QStringLiteral(":/icons/hash.svg"));
    
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
        m_settings.appModule = AppModule::IsoVerifier;
        applyAppModule();
        saveSettings();
        if (m_isoWidget) {
            m_isoWidget->refreshCatalogStatus();
        }
        onSettingsClicked();
    });
    status->addPermanentWidget(m_catalogStatusBtn);

    m_hashStatusLabel = new QLabel;
    m_hashStatusLabel->setStyleSheet(QString("color: %1;").arg(
        FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary)));
    status->addPermanentWidget(m_hashStatusLabel);
}

void MainWindow::initializeBackend()
{
    if (!Policy::PolicyServiceLocator::hasGateway()) {
        Policy::PolicyServiceLocator::install(Policy::PolicyGateway::createDefault());
    }

    // Create device monitor
    m_deviceMonitor = std::make_unique<DeviceMonitor>(this);
    
    // Create hash worker
    m_hashWorker = std::make_unique<HashWorker>(this);

    m_manifestWorker = std::make_unique<ManifestWorker>(this);
    
    // Create database manager (trust data via policy gateway / policyd)
    m_database = std::make_unique<DatabaseManager>(this);
    m_database->initialize();
    BlockedDriveStore::instance().refreshFromGateway();
    
    // Create mount manager
    m_mountManager = std::make_unique<MountManager>(this);
    
    // Create tray icon
    m_trayIcon = std::make_unique<TrayIcon>(this);

    m_hidMonitor = std::make_unique<HidDeviceMonitor>(this);
#ifdef Q_OS_WIN
    m_usbHostMonitor = std::make_unique<UsbHostMonitor>(this);
#endif
    m_badUsbBaselineStore = std::make_unique<BadUsbBaselineStore>(this);
    m_badUsbBaselineStore->initialize();
    m_usbmonCapture = std::make_unique<UsbmonCapture>(this);
    m_usbPcapInstaller = std::make_unique<UsbPcapInstaller>(this);
    connect(m_usbPcapInstaller.get(), &UsbPcapInstaller::stateChanged, this,
            [this](bool installed, const QString& status, const QString& path) {
                if (m_badUsbWidget) {
                    m_badUsbWidget->setPacketCaptureState(
                        installed, m_usbPcapInstaller && m_usbPcapInstaller->isWaitingForInstall(),
                        status);
                }
                if (installed && !path.isEmpty()) {
                    logMessage(QStringLiteral("USBPcap ready: %1").arg(path), LogLevel::Info);
                }
            });
    connect(m_usbPcapInstaller.get(), &UsbPcapInstaller::installFlowMessage, this,
            [this](const QString& message) {
                logMessage(message, LogLevel::Info);
                if (m_badUsbWidget) {
                    m_badUsbWidget->setCaptureStatus(message);
                }
            });
    connect(m_usbPcapInstaller.get(), &UsbPcapInstaller::installFlowFailed, this,
            [this](const QString& error) {
                logMessage(error, LogLevel::Warning);
                if (m_badUsbWidget) {
                    m_badUsbWidget->setCaptureStatus(error);
                }
            });
    if (m_badUsbWidget) {
        m_badUsbWidget->setBaselineCount(m_badUsbBaselineStore->allDevices().size());
    }
    refreshUsbPcapIntegration();
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

#ifdef Q_OS_WIN
    if (m_usbHostMonitor) {
        connect(m_usbHostMonitor.get(), &UsbHostMonitor::usbHostConnected, this,
                [this](const UsbHostDeviceInfo& device) {
                    m_deviceConnectedAt.insert(device.instanceId, QDateTime::currentDateTime());
                    scheduleUsbMonitorRefresh();
                    logMessage(QStringLiteral("USB attached: %1 (%2)")
                                   .arg(device.displayName, device.category),
                               LogLevel::Info);
                });
        connect(m_usbHostMonitor.get(), &UsbHostMonitor::usbHostDisconnected, this,
                [this](const QString& instanceId) {
                    m_deviceDisconnectedAt.insert(instanceId, QDateTime::currentDateTime());
                    scheduleUsbMonitorRefresh();
                });
        connect(m_usbHostMonitor.get(), &UsbHostMonitor::usbHostChanged, this,
                [this](const UsbHostDeviceInfo&) { scheduleUsbMonitorRefresh(); });
        connect(m_usbHostMonitor.get(), &UsbHostMonitor::initialScanComplete, this,
                [this](int count) {
                    logMessage(QStringLiteral("USB host scan complete: %1 device(s)").arg(count),
                               LogLevel::Info);
                    if (m_settings.diagnosticLogHostUsbInventory) {
                        AppDiagnostics::appendHostUsbInventorySnapshot(
                            m_usbHostMonitor->connectedDevices(), QStringLiteral("initial_scan"));
                    }
                    scheduleUsbMonitorRefresh();
                });
    }
#endif

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
    connect(m_mountManager.get(), &MountManager::mountStatusChanged, this,
            [this](const QString& deviceNode, bool mounted, const QString& mountPoint) {
                if (auto info = m_deviceMonitor->getDevice(deviceNode)) {
                    DeviceInfo updated = *info;
                    updated.isMounted = mounted;
                    updated.mountPoint = mounted ? mountPoint : QString();
                    updateDeviceCard(updated);
                }
                if (m_deviceMonitor) {
                    m_deviceMonitor->rescan();
                }
                refreshUsbMonitorHome();
            });
    
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
    m_settings.recentEventsLimit =
        m_qsettings->value("ui/recentEventsLimit", m_settings.recentEventsLimit).toInt();
    m_settings.deviceHistoryRetentionDays =
        m_qsettings->value("ui/deviceHistoryRetentionDays", m_settings.deviceHistoryRetentionDays)
            .toInt();
    m_settings.deviceHistoryMaxEntries =
        m_qsettings->value("ui/deviceHistoryMaxEntries", m_settings.deviceHistoryMaxEntries)
            .toInt();
    m_settings.allowedCountMode = allowedCountModeFromString(
        m_qsettings->value("security/allowedCountMode", QStringLiteral("trust_or_hash")).toString());
    m_settings.diagnosticLogHostUsbInventory =
        m_qsettings->value("diagnostics/logHostUsbInventory", true).toBool();
    m_settings.showExternalUsbPeripherals =
        m_qsettings->value("diagnostics/showExternalUsbPeripherals", true).toBool();
    m_settings.crashReportsEnabled =
        m_qsettings->value("diagnostics/crashReportsEnabled", false).toBool();
    m_settings.defaultTrustLevel =
        m_qsettings->value("security/defaultTrustLevel", m_settings.defaultTrustLevel).toInt();
    m_maxUiEvents = qMax(20, m_settings.recentEventsLimit);
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
    m_settings.fontSizePt = m_qsettings->value("appearance/fontSizePt", 10).toInt();
    FSStyle.setBaseFontSize(m_settings.fontSizePt);
    
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

    if (m_settingsPage) {
        m_settingsPage->loadSettings(m_settings);
        if (m_database) {
            m_settingsPage->setDatabaseStatistics(m_database->deviceCount(),
                                                    m_database->databasePath());
        }
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
    m_qsettings->setValue("security/defaultTrustLevel", m_settings.defaultTrustLevel);
    m_qsettings->setValue("security/allowedCountMode",
                          allowedCountModeToString(m_settings.allowedCountMode));
    m_qsettings->setValue("hashing/algorithm", m_settings.hashAlgorithm);
    m_qsettings->setValue("hashing/bufferSizeKB", m_settings.hashBufferSizeKB);
    m_qsettings->setValue("hashing/useMemoryMapping", m_settings.useMemoryMapping);
    m_qsettings->setValue("hashing/maxConcurrent", m_settings.maxConcurrentHashes);
    m_qsettings->setValue("hashing/defaultScope", hashScopeToString(m_settings.defaultHashScope));
    m_qsettings->setValue("hashing/defaultScanMode", hashScanModeToString(m_settings.defaultHashScanMode));
    m_qsettings->setValue("hashing/resumeCheckpoints", m_settings.hashResumeCheckpoints);
    m_qsettings->setValue("hashing/promptOnManual", m_settings.promptHashOptionsOnManual);
    m_qsettings->setValue("appearance/animations", m_settings.animationsEnabled);
    m_qsettings->setValue("appearance/fontSizePt", m_settings.fontSizePt);
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
    m_qsettings->setValue("ui/recentEventsLimit", m_settings.recentEventsLimit);
    m_qsettings->setValue("ui/deviceHistoryRetentionDays", m_settings.deviceHistoryRetentionDays);
    m_qsettings->setValue("ui/deviceHistoryMaxEntries", m_settings.deviceHistoryMaxEntries);
    m_qsettings->setValue("diagnostics/logHostUsbInventory", m_settings.diagnosticLogHostUsbInventory);
    m_qsettings->setValue("diagnostics/showExternalUsbPeripherals", m_settings.showExternalUsbPeripherals);
    m_qsettings->setValue("diagnostics/crashReportsEnabled", m_settings.crashReportsEnabled);
    m_qsettings->setValue("appearance/theme", FSStyle.themeName(FSStyle.currentTheme()));
    m_qsettings->setValue("window/geometry", saveGeometry());
    
    m_qsettings->sync();
}

void MainWindow::setupShortcuts()
{
    auto* refreshSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(refreshSc, &QShortcut::activated, this, &MainWindow::onRefreshClicked);

    auto* settingsSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(settingsSc, &QShortcut::activated, this, &MainWindow::onSettingsClicked);

    auto* quitSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q), this);
    connect(quitSc, &QShortcut::activated, this, &MainWindow::onQuitRequested);

    auto* hideSc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(hideSc, &QShortcut::activated, this, [this]() {
        if (shouldMinimizeToTray()) {
            hide();
            m_trayIcon->updateWindowVisibility(false);
        }
    });
}

void MainWindow::applyStyle()
{
    setStyleSheet(FSStyle.mainWindowStyleSheet());
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

#ifdef Q_OS_WIN
    if (!m_volumeDeviceNotify && internalWinId()) {
        DEV_BROADCAST_DEVICEINTERFACE filter = {};
        filter.dbcc_size = sizeof(filter);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        filter.dbcc_classguid = GUID_DEVINTERFACE_VOLUME;
        m_volumeDeviceNotify = RegisterDeviceNotification(
            reinterpret_cast<HDEVNOTIFY>(winId()), &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
    }
#endif

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

#ifdef Q_OS_WIN
void MainWindow::scheduleWinDeviceRescan()
{
    if (m_winDeviceRescanTimer) {
        m_winDeviceRescanTimer->start();
    }
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_DEVICECHANGE) {
            switch (msg->wParam) {
            case DBT_DEVICEARRIVAL:
            case DBT_DEVICEREMOVECOMPLETE:
            case DBT_DEVNODES_CHANGED:
                scheduleWinDeviceRescan();
                break;
            default:
                break;
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

// ============================================================================
// Device Events
// ============================================================================

void MainWindow::onDeviceConnected(const DeviceInfo& device)
{
    m_deviceConnectedAt.insert(device.deviceNode, QDateTime::currentDateTime());
    m_deviceDisconnectedAt.remove(device.deviceNode);
    logMessage(QString("Device connected: %1 (%2)").arg(device.displayName(), device.deviceNode),
               LogLevel::Info, device.deviceNode);
    
    // Add device card
    addDeviceCard(device);
    
    // Check if device is known
    if (m_database->hasDevice(device)) {
        auto record = m_database->getDevice(device);
        if (record) {
            handleKnownDevice(device, *record);
            m_trayIcon->notifyDeviceConnected(device, true);
        } else {
            logMessage(QStringLiteral("Whitelist entry could not be resolved for %1 — treating as new")
                           .arg(device.displayName()),
                       LogLevel::Warning, device.deviceNode);
            handleNewDevice(device);
            m_trayIcon->notifyDeviceConnected(device, false);
        }
    } else {
        handleNewDevice(device);
        m_trayIcon->notifyDeviceConnected(device, false);
    }
    
    updateSidebarStats();
    updateEmptyState();
    m_trayIcon->updateDeviceList(m_deviceMonitor->connectedDevices());
#ifdef Q_OS_WIN
    QTimer::singleShot(600, this, [this, deviceNode = device.deviceNode]() {
        if (auto info = m_deviceMonitor->getDevice(deviceNode)) {
            maybeTriggerIsoVerifyForMountedDevice(*info);
        }
    });
#else
    maybeTriggerIsoVerifyForMountedDevice(device);
#endif
    scheduleUsbMonitorRefresh();
    refreshAllowBlockListPage();
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

    m_deviceDisconnectedAt.insert(deviceNode, QDateTime::currentDateTime());
    logMessage(QString("Device disconnected: %1").arg(deviceName), LogLevel::Info, deviceNode);
    
    // Cancel any pending hash for this device
    for (auto it = m_hashJobDevices.begin(); it != m_hashJobDevices.end(); ++it) {
        if (it.value() == deviceNode) {
            m_hashWorker->cancelHash(it.key());
            break;
        }
    }

    for (auto it = m_manifestJobDevices.begin(); it != m_manifestJobDevices.end();) {
        if (it.value() == deviceNode) {
            m_manifestWorker->cancelJob(it.key());
            it = m_manifestJobDevices.erase(it);
        } else {
            ++it;
        }
    }
    m_manifestWorker->cancelJobsForDevice(deviceNode);
    
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
            if (card) {
                unblockDriveForDevice(card->device());
            }
            m_drivePromptInProgress.remove(drive);
        }
    }

    updateSidebarStats();
    updateEmptyState();
    
    m_trayIcon->notifyDeviceDisconnected(deviceName);
    m_trayIcon->updateDeviceList(m_deviceMonitor->connectedDevices());
    scheduleUsbMonitorRefresh();
    refreshAllowBlockListPage();
}

void MainWindow::onDeviceChanged(const DeviceInfo& device)
{
    updateDeviceCard(device);
#ifdef Q_OS_WIN
    QTimer::singleShot(600, this, [this, deviceNode = device.deviceNode]() {
        if (auto info = m_deviceMonitor->getDevice(deviceNode)) {
            maybeTriggerIsoVerifyForMountedDevice(*info);
        }
    });
#else
    maybeTriggerIsoVerifyForMountedDevice(device);
#endif
    scheduleUsbMonitorRefresh();
}

void MainWindow::onInitialScanComplete(int deviceCount)
{
    logMessage(QString("Initial scan complete: %1 device(s) found").arg(deviceCount), LogLevel::Info);
    updateSidebarStats();
    updateEmptyState();
    scheduleUsbMonitorRefresh();
    for (const DeviceInfo& device : m_deviceMonitor->connectedDevices()) {
#ifdef Q_OS_WIN
        QTimer::singleShot(600, this, [this, deviceNode = device.deviceNode]() {
            if (auto info = m_deviceMonitor->getDevice(deviceNode)) {
                maybeTriggerIsoVerifyForMountedDevice(*info);
            }
        });
#else
        maybeTriggerIsoVerifyForMountedDevice(device);
#endif
    }
}

void MainWindow::handleNewDevice(const DeviceInfo& device)
{
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::NewDevice);
    }

    const bool drivePromptActive =
        m_drivePromptInProgress.contains(DeviceDriveUtil::driveKey(device));
    const auto plan = DeviceTrustCoordinator::planNewDevice(
        device, m_settings.promptPerPartition, m_settings.requireConfirmationForNew,
        drivePromptActive, m_deviceMonitor->connectedDevices(), *m_database);

    switch (plan.action) {
    case DeviceTrustCoordinator::NewDeviceAction::DriveBlocked:
        logMessage(QString("Drive blocked: %1").arg(device.displayName()), LogLevel::Warning);
        return;
    case DeviceTrustCoordinator::NewDeviceAction::SkipDuplicatePrompt:
        return;
    case DeviceTrustCoordinator::NewDeviceAction::WhitelistPartition:
    case DeviceTrustCoordinator::NewDeviceAction::PromptPartition:
        handleNewDevicePartition(device);
        return;
    case DeviceTrustCoordinator::NewDeviceAction::WhitelistDrive:
        whitelistDrivePartitions(device);
        return;
    case DeviceTrustCoordinator::NewDeviceAction::PromptDrive: {
        m_drivePromptInProgress.insert(plan.driveKey);
        const bool allowed = showNewDriveDialog(device);
        m_drivePromptInProgress.remove(plan.driveKey);
        if (allowed) {
            whitelistDrivePartitions(device);
        } else {
            blockDriveForDevice(device);
            logMessage(QString("Drive blocked: %1").arg(device.displayName()), LogLevel::Warning);
        }
        return;
    }
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

    const DeviceRecord record =
        DeviceWhitelistService::makeRecord(device, *m_database, m_settings.defaultTrustLevel);
    m_database->addDevice(record);
    logMessage(QString("Device whitelisted: %1").arg(device.displayName()));

    if (m_settings.autoHashOnConnect) {
        startDeviceVerification(device.deviceNode);
        hashAllPartitionsOnParent(device);
    }
}

QString MainWindow::driveKey(const DeviceInfo& device) const
{
    return DeviceDriveUtil::driveKey(device);
}

bool MainWindow::isDriveKnown(const DeviceInfo& device) const
{
    return DeviceDriveUtil::isDriveKnown(device, m_deviceMonitor->connectedDevices(), *m_database);
}

void MainWindow::whitelistDrivePartitions(const DeviceInfo& device)
{
    const QString drive = driveKey(device);
    const auto result = DeviceTrustCoordinator::whitelistDrivePartitions(
        drive, m_deviceMonitor->connectedDevices(), *m_database, m_settings.defaultTrustLevel);

    for (const QString& deviceNode : result.whitelistedDeviceNodes) {
        if (DeviceCard* card = getDeviceCard(deviceNode)) {
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
    auto record = m_database->getDevice(*deviceInfo);
    if (!record) {
        record = m_database->getDevice(deviceId);
    }
    const QString storageId = record ? record->uniqueId : deviceId;
    
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
        if (m_database->verifyHash(*deviceInfo, result.hash)) {
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
        m_database->updateHash(storageId, result.hash, result.algorithm, result.durationMs,
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

#ifdef Q_OS_WIN
    if (!deviceInfo->mountPoint.isEmpty()) {
        onOpenMountPointRequested(deviceInfo->mountPoint);
        return;
    }
    showStyledInformation(
        this, QStringLiteral("No drive letter"),
        QStringLiteral(
            "Windows has not assigned a letter to this volume yet. Use Disk Management to assign "
            "one, or reconnect the USB drive."));
    return;
#endif

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

#ifdef Q_OS_WIN
    m_mountManager->unmount(deviceNode);
#else
    if (device->isMounted) {
        m_mountManager->unmount(deviceNode);
    }
    m_mountManager->powerOff(deviceNode);
#endif
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
    if (m_reportsPage) {
        refreshReportsPage();
    }
    const QString status = entry.status.toLower();
    if (m_alertsPage
        && (status == QLatin1String("fail") || status == QLatin1String("mismatch")
            || status == QLatin1String("error") || status == QLatin1String("partial"))) {
        refreshAlertsPage();
    }

    QString type = QStringLiteral("Verify");
    switch (entry.kind) {
        case VerifyHistoryKind::IsoScan: type = QStringLiteral("ISO"); break;
        case VerifyHistoryKind::Manifest: type = QStringLiteral("Watch"); break;
        case VerifyHistoryKind::Hash:
        default: break;
    }
    appendUiEvent(makeUiEvent(
        entry.summary.isEmpty() ? QStringLiteral("Verification") : entry.summary,
        entry.deviceLabel.isEmpty() ? entry.deviceNode : entry.deviceLabel, type, entry.status,
        entry.detail.isEmpty() ? entry.summary : entry.detail, entry.deviceNode));
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
    if (m_settingsPage) {
        m_settingsPage->loadSettings(m_settings);
        m_settingsPage->setDatabaseStatistics(m_database->deviceCount(),
                                              m_database->databasePath());
    }
    if (m_navSidebar) {
        m_navSidebar->setCurrentPage(AppPage::Settings);
    }
    onNavPageSelected(AppPage::Settings);
}

void MainWindow::applySettingsPage(const AppSettings& settings)
{
    const QString previousDbPath = m_database->databasePath();
    applyLiveSettings(settings);

    if (!m_settings.databasePath.isEmpty() && m_settings.databasePath != previousDbPath) {
        m_database->initialize(m_settings.databasePath);
        if (m_settingsPage) {
            m_settingsPage->setDatabaseStatistics(m_database->deviceCount(),
                                                  m_database->databasePath());
        }
    }
    refreshDeviceHistoryPage();
    logMessage(QStringLiteral("Settings saved"), LogLevel::Info);
}

void MainWindow::onThemeChanged(StyleManager::Theme theme)
{
    FSStyle.setTheme(theme);
    applyStyle();
    refreshShellStyles();

    for (auto* card : m_deviceCards) {
        card->setStyleSheet(FSStyle.deviceCardStyleSheet());
    }
}

void MainWindow::applySettings(const AppSettings& settings)
{
    m_maxUiEvents = qMax(20, settings.recentEventsLimit);
    while (m_uiEvents.size() > m_maxUiEvents) {
        m_uiEvents.removeLast();
    }
    m_trayIcon->setNotificationsEnabled(settings.showNotifications);
    m_hashWorker->setMaxConcurrent(settings.maxConcurrentHashes);
    FSStyle.setAnimationsEnabled(settings.animationsEnabled);
    FSStyle.setBaseFontSize(settings.fontSizePt);

    QString themeName = settings.theme;
    for (auto theme : FSStyle.availableThemes()) {
        if (FSStyle.themeName(theme) == themeName) {
            FSStyle.setTheme(theme);
            break;
        }
    }

    applyAppModule();
    applyIsoVerifyOptions();
    if (m_isoWidget) {
        m_isoWidget->setActiveProfile(settings.settingsProfile);
    }
    configureBadUsbMonitoring();
    refreshShellStyles();

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
        if (m_appModeStack && m_badUsbWidget) {
            m_appModeStack->setCurrentWidget(m_badUsbWidget);
        }
        if (m_searchEdit) {
            m_searchEdit->setVisible(false);
        }
        return;
    }
    if (m_modeTabBar->currentIndex() != index) {
        m_modeTabBar->blockSignals(true);
        m_modeTabBar->setCurrentIndex(index);
        m_modeTabBar->blockSignals(false);
    }
    if (m_searchEdit) {
        m_searchEdit->setVisible(index == 0);
    }
}

void MainWindow::onModeTabChanged(int index)
{
    if (index == 1) {
        m_settings.appModule = AppModule::IsoVerifier;
        m_appModeStack->setCurrentWidget(m_isoWidget);
    } else if (index == 2) {
        m_settings.appModule = AppModule::UsbMonitor;
        m_appModeStack->setCurrentWidget(m_watchListsPanel);
        refreshWatchListsPanel();
    } else {
        m_settings.appModule = AppModule::UsbMonitor;
        m_appModeStack->setCurrentIndex(0);
    }
    if (m_searchEdit) {
        m_searchEdit->setVisible(index == 0);
    }
    if (index != 2) {
        applyAppModule();
    }
    saveSettings();
}


void MainWindow::onNavPageSelected(AppPage page)
{
    if (!m_pageStack) {
        return;
    }
    const int index = appPageStackIndex(page);
    if (index >= 0 && index < m_pageStack->count()) {
        m_pageStack->setCurrentIndex(index);
    }
    if (page == AppPage::Settings && m_settingsPage) {
        m_settingsPage->loadSettings(m_settings);
        m_settingsPage->setDatabaseStatistics(m_database->deviceCount(),
                                              m_database->databasePath());
    }
    if (page == AppPage::UsbMonitor) {
        refreshUsbMonitorHome();
    }
    if (page == AppPage::DeviceHistory) {
        refreshDeviceHistoryPage();
    }
    if (page == AppPage::AllowBlockList) {
        refreshAllowBlockListPage();
    }
    if (page == AppPage::Alerts) {
        refreshAlertsPage();
    }
    if (page == AppPage::Reports) {
        refreshReportsPage();
    }
    if (page == AppPage::About) {
        refreshAboutPage();
    }
    if (page == AppPage::BadUsbMonitor) {
        refreshUsbPcapIntegration();
    }
    if (page == AppPage::IsoVerifier && m_isoWidget) {
        m_isoWidget->refreshCatalogStatus();
    }
}

void MainWindow::persistTimelineEvent(const UiEventEntry& entry)
{
    if (entry.deviceNode.isEmpty()) {
        return;
    }
    DeviceTimelineLog::instance().append(entry);
}

void MainWindow::appendUiEvent(const UiEventEntry& entry)
{
    m_uiEvents.prepend(entry);
    while (m_uiEvents.size() > m_maxUiEvents) {
        m_uiEvents.removeLast();
    }
    persistTimelineEvent(entry);
    refreshUsbMonitorHome();
    if (m_alertsPage && isAlertUiEvent(entry)) {
        refreshAlertsPage();
    }
}

QList<UiEventEntry> MainWindow::deviceHistoryEvents(const QString& deviceNode) const
{
    QList<UiEventEntry> out = DeviceTimelineLog::instance().entriesForDevice(
        deviceNode, m_settings.deviceHistoryRetentionDays, m_settings.deviceHistoryMaxEntries);

    auto timelineKey = [](const UiEventEntry& e) {
        return QStringLiteral("%1|%2|%3")
            .arg(e.time.toSecsSinceEpoch())
            .arg(e.event, e.result);
    };
    QSet<QString> seen;
    for (const UiEventEntry& e : out) {
        seen.insert(timelineKey(e));
    }

    const int verifyLimit = m_settings.deviceHistoryMaxEntries > 0
                                ? m_settings.deviceHistoryMaxEntries
                                : 500;
    for (const VerifyHistoryEntry& vh :
         VerifyHistory::instance().entriesForDevice(deviceNode, verifyLimit)) {
        UiEventEntry e;
        e.id = QStringLiteral("vh-%1-%2")
                   .arg(vh.timestamp.toSecsSinceEpoch())
                   .arg(vh.summary);
        e.time = vh.timestamp;
        e.event = vh.summary.isEmpty() ? QStringLiteral("Verification") : vh.summary;
        e.device = vh.deviceLabel.isEmpty() ? vh.deviceNode : vh.deviceLabel;
        switch (vh.kind) {
            case VerifyHistoryKind::IsoScan:
                e.type = QStringLiteral("ISO");
                break;
            case VerifyHistoryKind::Manifest:
                e.type = QStringLiteral("Watch");
                break;
            case VerifyHistoryKind::Hash:
            default:
                e.type = QStringLiteral("Verify");
                break;
        }
        e.result = vh.status;
        e.detail = vh.detail.isEmpty() ? vh.summary : vh.detail;
        e.deviceNode = vh.deviceNode;

        if (m_settings.deviceHistoryRetentionDays > 0) {
            const QDateTime cutoff =
                QDateTime::currentDateTime().addDays(-m_settings.deviceHistoryRetentionDays);
            if (e.time < cutoff) {
                continue;
            }
        }
        if (seen.contains(timelineKey(e))) {
            continue;
        }
        out.append(e);
        seen.insert(timelineKey(e));
    }

    std::sort(out.begin(), out.end(), [](const UiEventEntry& a, const UiEventEntry& b) {
        return a.time > b.time;
    });

    if (m_settings.deviceHistoryMaxEntries > 0 && out.size() > m_settings.deviceHistoryMaxEntries) {
        out = out.mid(0, m_settings.deviceHistoryMaxEntries);
    }
    return out;
}

void MainWindow::refreshDeviceHistoryPage()
{
    if (!m_deviceHistoryPage || !m_deviceMonitor) {
        return;
    }

    QStringList nodes;
    QStringList labels;
    auto addDevice = [this, &nodes, &labels](const QString& node, const QString& label) {
        if (node.isEmpty() || nodes.contains(node)) {
            return;
        }
        nodes.append(node);
        labels.append(label.isEmpty() ? node : label);
    };

    for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
        addDevice(d.deviceNode, m_userDeviceNames.value(d.deviceNode, d.displayName()));
    }
    for (const QString& node : DeviceTimelineLog::instance().knownDeviceNodes()) {
        QString label = node;
        if (auto info = m_deviceMonitor->getDevice(node)) {
            label = info->displayName();
        }
        addDevice(node, label);
    }
    for (const DeviceRecord& rec : m_database->getAllDevices()) {
        if (!rec.lastKnownInfo.deviceNode.isEmpty()) {
            addDevice(rec.lastKnownInfo.deviceNode, rec.lastKnownInfo.displayName());
        }
    }

    const QString current = m_deviceHistoryPage->selectedDeviceNode();
    m_deviceHistoryPage->setDeviceChoices(labels, nodes);
    if (!current.isEmpty() && nodes.contains(current)) {
        m_deviceHistoryPage->setSelectedDevice(current);
    } else if (!nodes.isEmpty()) {
        m_deviceHistoryPage->setSelectedDevice(nodes.first());
    }

    const QString selected = m_deviceHistoryPage->selectedDeviceNode();
    m_deviceHistoryPage->setEvents(deviceHistoryEvents(selected));
}

void MainWindow::showDeviceHistory(const QString& deviceNode)
{
    if (m_navSidebar) {
        m_navSidebar->setCurrentPage(AppPage::DeviceHistory);
    }
    if (m_deviceHistoryPage && !deviceNode.isEmpty()) {
        m_deviceHistoryPage->setSelectedDevice(deviceNode);
    }
    onNavPageSelected(AppPage::DeviceHistory);
}

bool MainWindow::isRecordCountedAsAllowed(const DeviceRecord& record) const
{
    switch (m_settings.allowedCountMode) {
        case AllowedCountMode::TrustLevel:
            return record.trustLevel >= 1;
        case AllowedCountMode::VerifiedHash:
            return !record.hash.isEmpty();
        case AllowedCountMode::TrustOrHash:
        default:
            return record.trustLevel >= 1 || !record.hash.isEmpty();
    }
}

bool MainWindow::isDriveBlocked(const DeviceInfo& device) const
{
    return DeviceDriveUtil::isDriveBlocked(device, *m_database);
}

void MainWindow::blockDriveForDevice(const DeviceInfo& device, const QString& label)
{
    const QString key = driveKey(device);
    const QString uid = m_database->canonicalUniqueId(device);
    const QString name = label.isEmpty() ? device.displayName() : label;
    BlockedDriveStore::instance().block(key, uid, name);
}

void MainWindow::unblockDriveForDevice(const DeviceInfo& device)
{
    BlockedDriveStore::instance().unblock(driveKey(device), m_database->canonicalUniqueId(device));
}

void MainWindow::allowDriveForDevice(const DeviceInfo& device)
{
    unblockDriveForDevice(device);
    if (!m_database->hasDevice(device)) {
        whitelistDrivePartitions(device);
    } else {
        const QString uid = m_database->canonicalUniqueId(device);
        m_database->setTrustLevel(uid, qMax(1, m_settings.defaultTrustLevel));
    }
}

void MainWindow::applyLiveSettings(const AppSettings& settings)
{
    m_settings = settings;
    applySettings(m_settings);
    saveSettings();
    if (m_settingsPage && m_database) {
        m_settingsPage->setDatabaseStatistics(m_database->deviceCount(),
                                              m_database->databasePath());
    }
    refreshUsbMonitorHome();
    refreshAllowBlockListPage();
}

void MainWindow::refreshShellStyles()
{
    applyStyle();
    if (m_navSidebar) {
        m_navSidebar->setStyleSheet(m_navSidebar->styleSheet());
    }
    if (m_usbMonitorPage) {
        m_usbMonitorPage->setStyleSheet(FSStyle.dataTableStyleSheet());
    }
    if (m_deviceHistoryPage) {
        m_deviceHistoryPage->setStyleSheet(QString());
    }
    if (m_allowBlockListPage) {
        m_allowBlockListPage->setStyleSheet(QString());
    }
    if (m_alertsPage) {
        m_alertsPage->setStyleSheet(QString());
    }
    if (m_reportsPage) {
        m_reportsPage->setStyleSheet(QString());
    }
    if (m_aboutPage) {
        m_aboutPage->setStyleSheet(QString());
    }
    if (m_settingsPage) {
        m_settingsPage->setStyleSheet(QString());
    }
}

void MainWindow::refreshAllowBlockListPage()
{
    if (!m_allowBlockListPage || !m_database) {
        return;
    }

    const QString filter = m_allowBlockListPage->currentFilterId();
    const QString search = m_allowBlockListPage->searchText().toLower();

    QHash<QString, AllowBlockRow> byId;
    auto mergeRow = [&](AllowBlockRow row) {
        if (!search.isEmpty()) {
            const QString hay = (row.displayName + row.vendorModel + row.uniqueId).toLower();
            if (!hay.contains(search)) {
                return;
            }
        }
        if (filter == QStringLiteral("allowed") && !row.isAllowed) {
            return;
        }
        if (filter == QStringLiteral("blocked") && !row.isBlocked) {
            return;
        }
        if (filter == QStringLiteral("unknown") && (row.isAllowed || row.isBlocked)) {
            return;
        }
        byId.insert(row.uniqueId.isEmpty() ? row.driveKey : row.uniqueId, row);
    };

    for (const DeviceRecord& rec : m_database->getAllDevices()) {
        AllowBlockRow row;
        row.uniqueId = rec.uniqueId;
        row.driveKey = rec.lastKnownInfo.deviceNode.isEmpty()
                           ? rec.uniqueId
                           : driveKey(rec.lastKnownInfo);
        row.displayName = rec.notes.isEmpty() ? rec.lastKnownInfo.displayName() : rec.notes;
        row.vendorModel = QStringLiteral("%1 / %2")
                              .arg(rec.lastKnownInfo.vendor, rec.lastKnownInfo.model);
        row.isBlocked =
            BlockedDriveStore::instance().isBlocked(row.driveKey, row.uniqueId);
        row.isAllowed = isRecordCountedAsAllowed(rec) && !row.isBlocked;
        row.status = row.isBlocked ? QStringLiteral("Blocked")
                                   : (row.isAllowed ? QStringLiteral("Allowed")
                                                    : QStringLiteral("Unknown"));
        row.trustDetail = rec.trustLevel >= 1
                              ? QStringLiteral("Trust %1").arg(rec.trustLevel)
                              : (rec.hash.isEmpty() ? QStringLiteral("No hash")
                                                    : QStringLiteral("Hash on file"));
        mergeRow(row);
    }

    for (const BlockedDriveEntry& be : BlockedDriveStore::instance().entries()) {
        if (byId.contains(be.uniqueId.isEmpty() ? be.driveKey : be.uniqueId)) {
            continue;
        }
        AllowBlockRow row;
        row.uniqueId = be.uniqueId;
        row.driveKey = be.driveKey;
        row.displayName = be.label.isEmpty() ? be.driveKey : be.label;
        row.isBlocked = true;
        row.isAllowed = false;
        row.status = QStringLiteral("Blocked");
        row.trustDetail = QStringLiteral("Blocked %1")
                              .arg(be.blockedAt.toString(QStringLiteral("yyyy-MM-dd")));
        mergeRow(row);
    }

    if (m_deviceMonitor) {
        for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
            const QString uid = m_database->canonicalUniqueId(d);
            AllowBlockRow row;
            if (byId.contains(uid)) {
                row = byId.value(uid);
            } else {
                row.uniqueId = uid;
                row.driveKey = driveKey(d);
                row.displayName = d.displayName();
                row.vendorModel = QStringLiteral("%1 / %2").arg(d.vendor, d.model);
                row.isBlocked = isDriveBlocked(d);
                auto rec = m_database->getDevice(d);
                row.isAllowed = rec && isRecordCountedAsAllowed(*rec) && !row.isBlocked;
                row.status = row.isBlocked ? QStringLiteral("Blocked")
                                           : (row.isAllowed ? QStringLiteral("Allowed")
                                                            : QStringLiteral("Unknown"));
                row.trustDetail = rec ? QStringLiteral("In database") : QStringLiteral("Not listed");
            }
            row.deviceNode = d.deviceNode;
            row.isConnected = true;
            mergeRow(row);
        }
    }

    QList<AllowBlockRow> rows = byId.values();
    std::sort(rows.begin(), rows.end(), [](const AllowBlockRow& a, const AllowBlockRow& b) {
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });

    int allowed = 0;
    int blocked = 0;
    for (const AllowBlockRow& r : rows) {
        if (r.isBlocked) {
            ++blocked;
        } else if (r.isAllowed) {
            ++allowed;
        }
    }
    m_allowBlockListPage->setSummary(allowed, blocked, rows.size());
    m_allowBlockListPage->setRows(rows);
}

QList<UiEventEntry> MainWindow::collectAlertEntries() const
{
    QList<UiEventEntry> alerts;
    for (const UiEventEntry& e : m_uiEvents) {
        if (isAlertUiEvent(e)) {
            alerts.append(e);
        }
    }

    const QList<VerifyHistoryEntry> history = VerifyHistory::instance().recentEntries(200);
    for (const VerifyHistoryEntry& vh : history) {
        const QString s = vh.status.toLower();
        if (s == QLatin1String("pass") || s.isEmpty()) {
            continue;
        }
        UiEventEntry e;
        e.id = QStringLiteral("vh-%1").arg(vh.timestamp.toSecsSinceEpoch());
        e.time = vh.timestamp;
        e.event = vh.summary.isEmpty() ? QStringLiteral("Verification") : vh.summary;
        e.device = vh.deviceLabel.isEmpty() ? vh.deviceNode : vh.deviceLabel;
        switch (vh.kind) {
            case VerifyHistoryKind::IsoScan:
                e.type = QStringLiteral("ISO");
                break;
            case VerifyHistoryKind::Manifest:
                e.type = QStringLiteral("Watch");
                break;
            case VerifyHistoryKind::Hash:
            default:
                e.type = QStringLiteral("Verify");
                break;
        }
        e.result = vh.status;
        e.detail = vh.detail.isEmpty() ? vh.summary : vh.detail;
        e.deviceNode = vh.deviceNode;
        alerts.append(e);
    }

    std::sort(alerts.begin(), alerts.end(), [](const UiEventEntry& a, const UiEventEntry& b) {
        return a.time > b.time;
    });

    QSet<QString> seen;
    QList<UiEventEntry> deduped;
    for (const UiEventEntry& e : alerts) {
        const QString key = QStringLiteral("%1|%2|%3|%4")
                                .arg(e.time.toSecsSinceEpoch())
                                .arg(e.event, e.device, e.result);
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        deduped.append(e);
    }
    return deduped;
}

void MainWindow::refreshAlertsPage()
{
    if (!m_alertsPage) {
        return;
    }
    m_alertsPage->setAlerts(collectAlertEntries());
}

void MainWindow::refreshReportsPage()
{
    if (!m_reportsPage) {
        return;
    }

    const QString auditPath = AuditLog::logPath();
    const QString policyPath = Policy::PolicyPaths::auditLogPath();
    m_reportsPage->setLogPaths(auditPath, policyPath);
    m_reportsPage->setVerificationRows(VerifyHistory::instance().recentEntries(150));
    m_reportsPage->setAuditRows(readAuditLogTail(auditPath, 200, false));
    m_reportsPage->setPolicyAuditRows(readAuditLogTail(policyPath, 200, true));
}

void MainWindow::refreshAboutPage()
{
    if (!m_aboutPage || !m_database) {
        return;
    }
#ifdef FLASHSPARTAN_VERSION
    m_aboutPage->setVersion(QStringLiteral(FLASHSPARTAN_VERSION));
#else
    m_aboutPage->setVersion(QStringLiteral("1.4.2"));
#endif
    m_aboutPage->setDatabaseSummary(m_database->deviceCount(), m_database->databasePath());
    m_aboutPage->setRuntimeInfo(QString::fromUtf8(qVersion()), QSysInfo::prettyProductName());
}

void MainWindow::scheduleUsbMonitorRefresh()
{
    if (m_usbMonitorRefreshTimer) {
        m_usbMonitorRefreshTimer->start();
    }
}

void MainWindow::refreshUsbMonitorHome()
{
    if (!m_usbMonitorPage || !m_deviceMonitor) {
        return;
    }

    int allowed = 0;
    for (const DeviceRecord& rec : m_database->getAllDevices()) {
        if (isRecordCountedAsAllowed(rec)) {
            ++allowed;
        }
    }

    QList<UsbDeviceRow> rows;
    QSet<QString> listedKeys;

    const auto formatCapacity = [](uint64_t sizeBytes) -> QString {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        double value = static_cast<double>(sizeBytes);
        int unit = 0;
        while (value >= 1024.0 && unit < 4) {
            value /= 1024.0;
            ++unit;
        }
        return QStringLiteral("%1 %2").arg(value, 0, 'f', unit > 0 ? 2 : 0).arg(units[unit]);
    };

    for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
        UsbDeviceRow row;
        row.deviceNode = d.deviceNode;
        row.displayName = m_userDeviceNames.value(d.deviceNode, d.displayName());
        if (auto rec = m_database->getDevice(d)) {
            if (!rec->notes.isEmpty()) {
                row.displayName = rec->notes;
            }
        }
        row.type = d.fsType.isEmpty() ? QStringLiteral("USB storage volume") : d.fsType;
        if (DeviceCard* card = getDeviceCard(d.deviceNode)) {
            row.status = verificationStatusToString(card->verificationStatus());
        } else if (m_database->hasDevice(d)) {
            row.status = QStringLiteral("Known");
        } else {
            row.status = QStringLiteral("New");
        }
        row.capacity = formatCapacity(d.sizeBytes);
        row.vendorModel = QStringLiteral("%1 / %2").arg(d.vendor, d.model);
        row.connectedAt =
            m_deviceConnectedAt.value(d.deviceNode).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        row.disconnectedAt =
            m_deviceDisconnectedAt.value(d.deviceNode).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        row.isConnected = true;
        listedKeys.insert(QStringLiteral("vol:") + d.deviceNode);
        rows.append(row);
    }

#ifdef Q_OS_WIN
    if (m_hidMonitor) {
        for (const HidDeviceInfo& h : m_hidMonitor->connectedDevices()) {
            const QString key = QStringLiteral("hid:") + h.stableId();
            if (listedKeys.contains(key)) {
                continue;
            }
            UsbDeviceRow row;
            row.deviceNode = key;
            row.displayName = h.displayName();
            row.type = h.capabilities.isEmpty()
                           ? QStringLiteral("HID / security key")
                           : h.capabilities.join(QStringLiteral(", "));
            row.status = QStringLiteral("Connected");
            row.capacity = QStringLiteral("—");
            row.vendorModel = QStringLiteral("%1 / %2")
                                  .arg(h.manufacturer.isEmpty() ? h.vendorId : h.manufacturer,
                                       h.product.isEmpty() ? h.productId : h.product);
            row.connectedAt =
                m_deviceConnectedAt.value(key).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
            row.disconnectedAt =
                m_deviceDisconnectedAt.value(key).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
            row.isConnected = true;
            listedKeys.insert(key);
            rows.append(row);
        }
    }

    int internalUsbTracked = 0;
    if (m_usbHostMonitor) {
        for (const UsbHostDeviceInfo& u : m_usbHostMonitor->connectedDevices()) {
            if (u.isInternalHost()) {
                ++internalUsbTracked;
                continue;
            }
            if (!m_settings.showExternalUsbPeripherals) {
                continue;
            }
            const QString key = QStringLiteral("usb:") + u.instanceId;
            if (listedKeys.contains(key)) {
                continue;
            }
            UsbDeviceRow row;
            row.deviceNode = key;
            row.displayName = u.displayName;
            row.type = u.category;
            row.status = QStringLiteral("Connected");
            row.capacity = QStringLiteral("—");
            row.vendorModel = QStringLiteral("%1 / %2")
                                  .arg(u.manufacturer.isEmpty() ? u.vendorId : u.manufacturer,
                                       u.productId.isEmpty() ? QStringLiteral("—") : u.productId);
            row.connectedAt =
                m_deviceConnectedAt.value(u.instanceId).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
            row.disconnectedAt =
                m_deviceDisconnectedAt.value(u.instanceId).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
            row.isConnected = true;
            row.nameEditable = false;
            listedKeys.insert(key);
            rows.append(row);
        }
    }
#endif

    UsbMonitorStats stats;
    stats.connected = m_deviceMonitor->connectedDevices().size();
    stats.allowed = allowed;
    stats.blocked = BlockedDriveStore::instance().entries().size();
    stats.events = m_uiEvents.size();
#ifdef Q_OS_WIN
    stats.internalUsbTracked = internalUsbTracked;
#endif
    m_usbMonitorPage->setStats(stats);
    m_usbMonitorPage->setDevices(rows);
#ifdef Q_OS_WIN
    m_usbMonitorPage->setInternalUsbNote(stats.internalUsbTracked, AppDiagnostics::hostUsbInventoryPath());
#endif
    m_usbMonitorPage->setEvents(m_uiEvents);
}

void MainWindow::showDeviceActionsMenu(const QString& deviceNode)
{
    if (deviceNode.startsWith(QStringLiteral("hid:"))
        || deviceNode.startsWith(QStringLiteral("usb:"))) {
        QMenu menu(this);
        menu.setStyleSheet(FSStyle.menuStyleSheet());
        menu.addAction(QStringLiteral("Refresh list"), [this]() {
            if (m_deviceMonitor) {
                m_deviceMonitor->rescan();
            }
#ifdef Q_OS_WIN
            if (m_usbHostMonitor) {
                m_usbHostMonitor->rescan();
            }
            if (m_hidMonitor) {
                m_hidMonitor->rescan();
            }
#endif
            refreshUsbMonitorHome();
        });
#ifdef Q_OS_WIN
        menu.addAction(QStringLiteral("Export USB inventory log…"), [this]() {
            if (m_usbHostMonitor && m_settings.diagnosticLogHostUsbInventory) {
                AppDiagnostics::appendHostUsbInventorySnapshot(
                    m_usbHostMonitor->connectedDevices(), QStringLiteral("manual_export"));
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(AppDiagnostics::logsDir()));
        });
#endif
        menu.exec(QCursor::pos());
        return;
    }

    QMenu menu(this);
    menu.setStyleSheet(FSStyle.menuStyleSheet());
#ifdef Q_OS_WIN
    menu.addAction(QStringLiteral("Open drive"), [this, deviceNode]() { onMountRequested(deviceNode); });
    menu.addAction(QStringLiteral("Open folder"), [this, deviceNode]() {
        if (auto info = m_deviceMonitor->getDevice(deviceNode)) {
            if (!info->mountPoint.isEmpty()) {
                onOpenMountPointRequested(info->mountPoint);
            }
        }
    });
#else
    menu.addAction(QStringLiteral("Mount"), [this, deviceNode]() { onMountRequested(deviceNode); });
    menu.addAction(QStringLiteral("Unmount"), [this, deviceNode]() { onUnmountRequested(deviceNode); });
    menu.addAction(QStringLiteral("Open folder"), [this, deviceNode]() {
        if (auto info = m_deviceMonitor->getDevice(deviceNode)) {
            if (!info->mountPoint.isEmpty()) {
                onOpenMountPointRequested(info->mountPoint);
            }
        }
    });
#endif
    menu.addAction(QStringLiteral("Watch folders…"), [this, deviceNode]() { onWatchListRequested(deviceNode); });
    menu.addAction(QStringLiteral("Rehash"), [this, deviceNode]() { onRehashRequested(deviceNode); });
    menu.addSeparator();
    menu.addAction(Platform::isWindows() ? QStringLiteral("Eject drive")
                                         : QStringLiteral("Eject"),
                     [this, deviceNode]() { onEjectRequested(deviceNode); });
    menu.exec(QCursor::pos());
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
    if (m_hiddenDeviceLayout) {
        m_hiddenDeviceLayout->addWidget(card);
    }
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
#ifndef Q_OS_WIN
        if (mounted && hashDeviceNode == uiDeviceNode) {
            logMessage(QString("Unmounting %1 before hash verification").arg(uiDeviceNode));
            m_unmountBeforeHash.insert(uiDeviceNode);
            m_pendingHashLaunch[uiDeviceNode] = {hashDeviceNode, scope, mode, resume};
            m_mountManager->unmount(uiDeviceNode);
            return;
        }
#else
        Q_UNUSED(mounted);
        if (scope == HashScope::Partition && mounted && !deviceInfo->mountPoint.isEmpty()) {
            QString dismountError;
            if (WinStorage::dismountVolumeRootInPlace(deviceInfo->mountPoint, &dismountError)) {
                logMessage(QStringLiteral("Dismounted %1 in place for hashing (drive letter kept)")
                               .arg(deviceInfo->mountPoint),
                           LogLevel::Info);
            } else if (!dismountError.isEmpty()) {
                logMessage(QStringLiteral("Could not dismount %1 before hash: %2 — trying read with "
                                          "elevation if needed")
                               .arg(deviceInfo->mountPoint, dismountError),
                           LogLevel::Warning);
            }
        }
#endif
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

void MainWindow::logMessage(const QString& message, LogLevel level, const QString& deviceNode)
{
    QString prefix;
    QString result;
    
    switch (level) {
        case LogLevel::Debug:
            prefix = "DEBUG";
            result = QStringLiteral("debug");
            break;
        case LogLevel::Info:
            prefix = "INFO";
            result = QStringLiteral("info");
            break;
        case LogLevel::Warning:
            prefix = "WARN";
            result = QStringLiteral("warn");
            break;
        case LogLevel::Error:
            prefix = "ERROR";
            result = QStringLiteral("error");
            break;
        case LogLevel::Security:
            prefix = "SECURITY";
            result = QStringLiteral("alert");
            break;
    }

    UiEventEntry ev = makeUiEvent(message, QStringLiteral("—"), QStringLiteral("System"), result,
                                 QStringLiteral("[%1] %2").arg(prefix, message), deviceNode);
    m_uiEvents.prepend(ev);
    persistTimelineEvent(ev);
    while (m_uiEvents.size() > m_maxUiEvents) {
        m_uiEvents.removeLast();
    }
    if (m_usbMonitorPage) {
        m_usbMonitorPage->setEvents(m_uiEvents);
        UsbMonitorStats stats;
        if (m_deviceMonitor) {
            stats.connected = m_deviceMonitor->connectedDevices().size();
        }
        stats.events = m_uiEvents.size();
        m_usbMonitorPage->setStats(stats);
    }
    if (m_alertsPage && isAlertUiEvent(ev)) {
        refreshAlertsPage();
    }

    if (m_logList) {
        const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        const QString logText = QString("[%1] %2").arg(timestamp, message);
        auto* item = new QListWidgetItem(logText);
        item->setForeground(QColor(FSStyle.color(StyleManager::ColorRole::TextSecondary)));
        m_logList->addItem(item);
        m_logList->scrollToBottom();
        while (m_logList->count() > 500) {
            delete m_logList->takeItem(0);
        }
    }

    qDebug() << QString("[%1] %2").arg(prefix, message);
}

void MainWindow::updateEmptyState()
{
    refreshUsbMonitorHome();
    if (!m_emptyStateLabel || !m_contentStack) {
        return;
    }
    if (m_deviceCards.isEmpty()) {
        const QString emptyText =
#ifdef Q_OS_WIN
            QStringLiteral(
                "No USB storage volumes on the home screen\n\n"
                "Plug in a flash drive (with a drive letter), security key, or other USB device. "
                "All USB attachments appear on the USB Monitor page.\n\n"
                "Use ISO Verify for image files, or watch folders for fast change detection.")
#else
            QStringLiteral(
                "No USB devices connected\n\n"
                "Connect a USB flash drive or switch to ISO Verify in Settings.\n\n"
                "Recommended: watch selected folders (fast) or automatic ISO checks — "
                "full-partition hashing is optional for advanced users.")
#endif
            ;
        m_emptyStateLabel->setText(emptyText);
        m_contentStack->setCurrentIndex(1);  // Empty state
    } else {
        m_contentStack->setCurrentIndex(0);  // Device list
    }
}

void MainWindow::refreshWatchListsPanel()
{
    if (!m_watchListsPanel || !m_deviceMonitor || !m_database) {
        return;
    }
    QList<DeviceInfo> devices;
    QHash<QString, int> groupCounts;
    for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
        devices.append(d);
        const QString id = canonicalDeviceId(d);
        int groups = 0;
        if (auto record = m_database->getDevice(id)) {
            groups = record->watchManifest.groups.size();
        }
        groupCounts.insert(d.deviceNode, groups);
    }
    m_watchListsPanel->refresh(devices, groupCounts);
}

void MainWindow::updateSidebarStats()
{
    if (m_connectedCountLabel) {
        m_connectedCountLabel->setText(QString::number(m_deviceCards.size()));
    }
    if (m_whitelistedCountLabel) {
        m_whitelistedCountLabel->setText(QString::number(m_database->deviceCount()));
    }
    if (m_hashingCountLabel) {
        m_hashingCountLabel->setText(QString::number(m_activeHashCount));
    }

    m_trayIcon->setDeviceCount(m_deviceCards.size(), m_database->deviceCount());
    refreshWatchListsPanel();
    refreshUsbMonitorHome();
    updateStatusBar();
}

bool MainWindow::showNewDriveDialog(const DeviceInfo& device)
{
    const auto summary =
        DeviceDriveUtil::summarizeDrive(device, m_deviceMonitor->connectedDevices());

    QString message = QString(
        "<b>Unknown USB drive detected:</b><br><br>"
        "<b>Drive:</b> %1<br>"
        "<b>Representative partition:</b> %2<br>"
        "<b>Serial:</b> %3<br>"
        "<b>Partitions detected:</b> %4<br>"
        "<b>Partition nodes:</b> %5<br><br>"
        "Add this <b>entire drive</b> to the whitelist and hash all partitions?")
        .arg(summary.driveKey)
        .arg(device.deviceNode)
        .arg(device.serial.isEmpty() ? "N/A" : device.serial)
        .arg(summary.partitionCount)
        .arg(summary.partitionNodes.join(", "));
    message += DeviceWhitelistService::weakIdentityNoticeHtml(device);

    return showStyledRichQuestion(this, QStringLiteral("New Drive Detected"), message)
           == QMessageBox::Yes;
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
    message += DeviceWhitelistService::weakIdentityNoticeHtml(device);

    return showStyledRichQuestion(this, QStringLiteral("New Device Detected"), message)
           == QMessageBox::Yes;
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

    applyDialogStyle(msgBox);
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
    Q_UNUSED(deviceNode);
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



} // namespace FlashSpartan
