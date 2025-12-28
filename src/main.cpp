#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QTextStream>

#include <iostream>
#include <csignal>

#include "MainWindow.h"
#include "StyleManager.h"
#include "Types.h"

using namespace FlashSentry;

// Global pointer for signal handling
static MainWindow* g_mainWindow = nullptr;

// Custom message handler for logging
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    static QFile logFile;
    static QTextStream logStream;
    static bool initialized = false;
    
    if (!initialized) {
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(logDir);
        QString logPath = logDir + "/flashsentry.log";
        
        logFile.setFileName(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            logStream.setDevice(&logFile);
            initialized = true;
        }
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString level;
    
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO"; break;
        case QtWarningMsg:  level = "WARN"; break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }
    
    QString logMessage = QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(level, -5)
        .arg(msg);
    
    // Output to stderr
    std::cerr << logMessage.toStdString() << std::endl;
    
    // Output to log file
    if (initialized) {
        logStream << logMessage << "\n";
        logStream.flush();
    }
    
    if (type == QtFatalMsg) {
        abort();
    }
}

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
    static QSharedMemory sharedMemory("FlashSentry_SingleInstance_Lock");
    
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
    std::cout << "FlashSentry v1.0.0" << std::endl;
    std::cout << "USB Flash Drive Security Monitor" << std::endl;
    std::cout << "Built with Qt " << qVersion() << std::endl;
}

int main(int argc, char* argv[])
{
    // Set application attributes before creating QApplication
    QApplication::setApplicationName("FlashSentry");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("FlashSentry");
    QApplication::setOrganizationDomain("flashsentry.io");
    
    // Enable high DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    // Create application instance
    QApplication app(argc, argv);
    
    // Install custom message handler
    qInstallMessageHandler(messageHandler);
    
    // Parse command line arguments
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
    
    parser.process(app);
    
    // Check for single instance
    if (!parser.isSet(forceOption) && isAlreadyRunning()) {
        qWarning() << "Another instance of FlashSentry is already running.";
        qWarning() << "Use --force to start anyway.";
        
        QMessageBox::warning(
            nullptr,
            "FlashSentry Already Running",
            "Another instance of FlashSentry is already running.\n\n"
            "Check your system tray for the FlashSentry icon."
        );
        
        return 1;
    }
    
    // Check for system tray availability
    if (!parser.isSet(noTrayOption) && !QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available on this system.";
        qWarning() << "FlashSentry will run without tray icon.";
    }
    
    // Setup signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);
    
    // Register custom types
    qRegisterMetaType<DeviceInfo>("DeviceInfo");
    qRegisterMetaType<DeviceRecord>("DeviceRecord");
    qRegisterMetaType<HashResult>("HashResult");
    qRegisterMetaType<VerificationStatus>("VerificationStatus");
    qRegisterMetaType<LogLevel>("LogLevel");
    
    // Initialize style manager
    StyleManager::instance().initialize();
    StyleManager::instance().applyToApplication();
    
    // Create main window
    MainWindow mainWindow;
    g_mainWindow = &mainWindow;
    
    // Show window (or minimize based on settings/args)
    if (parser.isSet(minimizedOption)) {
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
    
    qInfo() << "FlashSentry started successfully";
    qInfo() << "Qt version:" << qVersion();
    qInfo() << "System:" << QSysInfo::prettyProductName();
    
    // Run the application event loop
    int result = app.exec();
    
    // Cleanup
    g_mainWindow = nullptr;
    
    qInfo() << "FlashSentry exiting with code" << result;
    
    return result;
}