#include <QApplication>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QFileInfo>
#include <QTimer>
#include <QSharedMemory>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QSettings>

#include <iostream>
#include <csignal>

#include "AppPaths.h"
#include "AppDiagnostics.h"
#include "CrashReporter.h"
#include "MainWindow.h"
#include "StyleManager.h"
#include "Types.h"
#include "VerifyCli.h"

using namespace FlashSpartan;

// Global pointer for signal handling
static MainWindow* g_mainWindow = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum)
{
    qInfo() << "Received signal" << signum << "- shutting down gracefully";
    
    if (g_mainWindow) {
        g_mainWindow->close();
    }
    
    QApplication::quit();
}

// Check if another instance is running
bool isAlreadyRunning()
{
    static QSharedMemory sharedMemory("FlashSpartan_SingleInstance_Lock");
    
    if (sharedMemory.attach()) {
        // Another instance is already running
        return true;
    }
    
    if (sharedMemory.create(1)) {
        // We are the first instance
        return false;
    }
    
    // Failed to create - might be stale, try to attach and detach
    if (sharedMemory.attach()) {
        sharedMemory.detach();
    }
    
    return !sharedMemory.create(1);
}

void printVersion()
{
#ifdef FLASHSPARTAN_VERSION
    std::cout << "FlashSpartan v" << FLASHSPARTAN_VERSION << std::endl;
#else
    std::cout << "FlashSpartan (version unknown)" << std::endl;
#endif
    std::cout << "USB Flash Drive Security Monitor" << std::endl;
    std::cout << "Built with Qt " << qVersion() << std::endl;
}

int main(int argc, char* argv[])
{
    QCoreApplication::setApplicationName("FlashSpartan");
#ifdef FLASHSPARTAN_VERSION
    QApplication::setApplicationVersion(QLatin1String(FLASHSPARTAN_VERSION));
#else
    QApplication::setApplicationVersion(QStringLiteral("1.1.5"));
#endif
    QCoreApplication::setOrganizationName("FlashSpartan");
    QCoreApplication::setOrganizationDomain("flashspartan.io");

    QCommandLineParser parser;
    parser.setApplicationDescription("USB Flash Drive Security Monitor");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption minimizedOption(
        {"m", "minimized"},
        "Start minimized to system tray"
    );
    parser.addOption(minimizedOption);
    
    QCommandLineOption forceOption(
        {"f", "force"},
        "Force start even if another instance is running"
    );
    parser.addOption(forceOption);
    
    QCommandLineOption debugOption(
        {"d", "debug"},
        "Enable debug output"
    );
    parser.addOption(debugOption);
    
    QCommandLineOption noTrayOption(
        "no-tray",
        "Disable system tray icon"
    );
    parser.addOption(noTrayOption);
    
    QCommandLineOption configOption(
        {"c", "config"},
        "Path to configuration file",
        "path"
    );
    parser.addOption(configOption);

    QCommandLineOption settingsOption(
        "settings",
        "Open the settings dialog on startup"
    );
    parser.addOption(settingsOption);

    QCommandLineOption verifyIsoOption(QStringLiteral("verify-iso"), QStringLiteral("Verify one image file and exit"), QStringLiteral("path"));
    QCommandLineOption verifyMountOption(QStringLiteral("verify-mount"), QStringLiteral("Verify images on mount point and exit"), QStringLiteral("path"));
    QCommandLineOption verifyDirOption(QStringLiteral("verify-dir"), QStringLiteral("Verify images in directory and exit"), QStringLiteral("path"));
    QCommandLineOption updateCatalogOption(QStringLiteral("update-catalog"), QStringLiteral("Refresh ISO catalog manifest from remote"));
    QCommandLineOption exportReportOption(QStringLiteral("export-report"), QStringLiteral("Verify path and print report"), QStringLiteral("path"));
    QCommandLineOption reportFormatOption(QStringLiteral("report-format"), QStringLiteral("Report format: text, csv, html, or json"), QStringLiteral("format"), QStringLiteral("text"));
    QCommandLineOption listPublishersOption(QStringLiteral("list-publishers"), QStringLiteral("List built-in ISO publisher IDs and exit"));
    QCommandLineOption trustHashOption(QStringLiteral("trust-hash"), QStringLiteral("Save user-trusted SHA-256 for a filename (TOFU)"), QStringLiteral("file:hash"));
    QCommandLineOption jsonOption(QStringLiteral("json"), QStringLiteral("Machine-readable JSON on stdout (verify/export commands)"));
    QCommandLineOption quietOption(QStringLiteral("quiet"), QStringLiteral("Print summary only (no per-file report body)"));
    parser.addOption(verifyIsoOption);
    parser.addOption(verifyMountOption);
    parser.addOption(verifyDirOption);
    parser.addOption(updateCatalogOption);
    parser.addOption(exportReportOption);
    parser.addOption(reportFormatOption);
    parser.addOption(listPublishersOption);
    parser.addOption(trustHashOption);
    parser.addOption(jsonOption);
    parser.addOption(quietOption);

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    AppPaths::migrateFromLegacyConfigIfNeeded();
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/flashspartan.svg")));
    AppDiagnostics::installQtMessageHandler();
    parser.process(app);

    if (parser.isSet(configOption)) {
        VerifyCli::setConfigFilePath(parser.value(configOption));
    }
    if (parser.isSet(jsonOption)) {
        VerifyCli::setJsonOutput(true);
    }
    if (parser.isSet(quietOption)) {
        VerifyCli::setQuietOutput(true);
    }

    if (parser.isSet(listPublishersOption)) {
        return VerifyCli::runListPublishers();
    }
    if (parser.isSet(trustHashOption)) {
        const QString spec = parser.value(trustHashOption);
        const int colon = spec.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            std::cerr << "trust-hash format: Win11.iso:abcdef...64hex\n";
            return VerifyCli::ExitError;
        }
        return VerifyCli::runTrustHash(spec.left(colon), spec.mid(colon + 1));
    }
    if (parser.isSet(updateCatalogOption)) {
        return VerifyCli::runUpdateCatalog(true);
    }
    if (parser.isSet(verifyIsoOption)) {
        return VerifyCli::runVerifyIso(parser.value(verifyIsoOption));
    }
    if (parser.isSet(verifyMountOption)) {
        return VerifyCli::runVerifyMount(parser.value(verifyMountOption));
    }
    if (parser.isSet(verifyDirOption)) {
        return VerifyCli::runVerifyDir(parser.value(verifyDirOption));
    }
    if (parser.isSet(exportReportOption)) {
        const QString path = parser.value(exportReportOption);
        const QString fmt = parser.value(reportFormatOption);
        return VerifyCli::runExportReport(path, fmt);
    }

    if (parser.isSet(debugOption)) {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    }

    if (parser.isSet(configOption)) {
        const QString configDir = QFileInfo(parser.value(configOption)).absolutePath();
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir);
    }
    
    // Check for single instance
    if (!parser.isSet(forceOption) && isAlreadyRunning()) {
        qWarning() << "Another instance of FlashSpartan is already running.";
        qWarning() << "Use --force to start anyway.";
        
        QMessageBox::warning(
            nullptr,
            "FlashSpartan Already Running",
            "Another instance of FlashSpartan is already running.\n\n"
            "Check your system tray for the FlashSpartan icon."
        );
        
        return 1;
    }
    
    // Check for system tray availability
    if (!parser.isSet(noTrayOption) && !QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available on this system.";
        qWarning() << "FlashSpartan will run without tray icon.";
    }
    
#if defined(Q_OS_UNIX)
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);
#endif
    
    // Register custom types
    qRegisterMetaType<DeviceInfo>("DeviceInfo");
    qRegisterMetaType<DeviceRecord>("DeviceRecord");
    qRegisterMetaType<HashResult>("HashResult");
    qRegisterMetaType<VerificationStatus>("VerificationStatus");
    qRegisterMetaType<LogLevel>("LogLevel");
    
    // Initialize style manager
    StyleManager::instance().initialize();
    StyleManager::instance().applyToApplication();
    
    AppSettings startupSettings;
    {
        QSettings settings(QStringLiteral("flashspartan"), QStringLiteral("FlashSpartan"));
        startupSettings.crashReportsEnabled =
            settings.value(QStringLiteral("diagnostics/crashReportsEnabled"), false).toBool();
    }
    CrashReporter::tryInstall(startupSettings);

    // Create main window
    MainWindow mainWindow;
    g_mainWindow = &mainWindow;

    if (parser.isSet(settingsOption)) {
        QTimer::singleShot(0, &mainWindow, &MainWindow::showSettingsDialog);
    }
    
    const bool startMinimized = parser.isSet(minimizedOption) || mainWindow.wantsStartMinimized();
    if (startMinimized) {
        // Start minimized
        if (QSystemTrayIcon::isSystemTrayAvailable() && !parser.isSet(noTrayOption)) {
            qInfo() << "Starting minimized to system tray";
            // Window will be hidden, tray icon shown
        } else {
            mainWindow.showMinimized();
        }
    } else {
        mainWindow.show();
    }
    
    qInfo() << "FlashSpartan started successfully";
    qInfo() << "Qt version:" << qVersion();
    qInfo() << "System:" << QSysInfo::prettyProductName();
    
    // Run the application event loop
    int result = app.exec();
    
    // Cleanup
    g_mainWindow = nullptr;
    
    qInfo() << "FlashSpartan exiting with code" << result;
    
    return result;
}