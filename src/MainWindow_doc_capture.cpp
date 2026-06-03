#include "MainWindow.h"
#include "AppNavigation.h"
#include "StyleManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <QUuid>

namespace FlashSpartan {

void MainWindow::seedDocumentationDemoState()
{
    if (m_usbMonitorPage) {
        QList<UsbDeviceRow> rows;
        UsbDeviceRow drive;
        drive.deviceNode = QStringLiteral("E:\\");
        drive.displayName = QStringLiteral("Demo USB Drive");
        drive.type = QStringLiteral("exFAT");
        drive.status = QStringLiteral("Verified");
        drive.capacity = QStringLiteral("28.90 GB");
        drive.vendorModel = QStringLiteral("Demo Vendor / Secure Stick 32G");
        drive.connectedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        drive.disconnectedAt = QStringLiteral("—");
        drive.isConnected = true;
        rows.append(drive);

        UsbMonitorStats stats;
        stats.connected = 1;
        stats.allowed = 2;
        stats.blocked = 0;
        stats.events = 3;
        stats.internalUsbTracked = 0;
        m_usbMonitorPage->setStats(stats);
        m_usbMonitorPage->setDevices(rows);

        auto makeEvent = [](const QString& event, const QString& result, const QString& detail) {
            UiEventEntry e;
            e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            e.time = QDateTime::currentDateTime();
            e.event = event;
            e.device = QStringLiteral("Demo USB Drive");
            e.type = QStringLiteral("USB storage");
            e.result = result;
            e.detail = detail;
            e.deviceNode = QStringLiteral("E:\\");
            return e;
        };
        QList<UiEventEntry> events;
        events.append(makeEvent(QStringLiteral("Device connected"), QStringLiteral("Info"), {}));
        events.append(makeEvent(QStringLiteral("Hash verified"), QStringLiteral("Pass"),
                                QStringLiteral("Partition hash matches whitelist")));
        events.append(makeEvent(QStringLiteral("ISO scan"), QStringLiteral("Pass"),
                                QStringLiteral("Catalog / sidecar check complete")));
        m_usbMonitorPage->setEvents(events);
    }

    if (m_allowBlockListPage) {
        refreshAllowBlockListPage();
    }

    if (m_isoWidget) {
        m_isoWidget->refreshCatalogStatus();
        const QStringList fixtureCandidates = {
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(
                QStringLiteral("../tests/fixtures/ventoy-scan")),
            QStringLiteral("/workspace/tests/fixtures/ventoy-scan"),
        };
        for (const QString& ventoy : fixtureCandidates) {
            if (QDir(ventoy).exists()) {
                m_isoWidget->setScanDirectory(ventoy);
                break;
            }
        }
    }

    refreshAboutPage();
    refreshShellStyles();
}

int MainWindow::captureDocumentationScreenshots(const QString& outputDir)
{
    m_settings.showFirstRunWizard = false;
    m_settings.startMinimized = false;

    const QDir out(outputDir);
    if (!out.exists() && !out.mkpath(QStringLiteral("."))) {
        qWarning() << "Cannot create screenshot directory:" << outputDir;
        return 1;
    }

    seedDocumentationDemoState();

    struct Shot {
        AppPage page;
        QString fileName;
    };
    const QList<Shot> shots = {
        {AppPage::UsbMonitor, QStringLiteral("usb-monitor.png")},
        {AppPage::IsoVerifier, QStringLiteral("iso-verifier.png")},
        {AppPage::AllowBlockList, QStringLiteral("allow-block-list.png")},
        {AppPage::Reports, QStringLiteral("reports.png")},
        {AppPage::Settings, QStringLiteral("settings.png")},
        {AppPage::About, QStringLiteral("about.png")},
        {AppPage::BadUsbMonitor, QStringLiteral("badusb-monitor.png")},
    };

    resize(1280, 820);
    show();
    QApplication::processEvents();
    QThread::msleep(300);

    int saved = 0;
    for (const Shot& shot : shots) {
        if (m_navSidebar) {
            m_navSidebar->setCurrentPage(shot.page);
        }
        onNavPageSelected(shot.page);
        QApplication::processEvents();
        QThread::msleep(250);

        const QString path = out.filePath(shot.fileName);
        if (grab().save(path, "PNG")) {
            qInfo() << "Saved" << path;
            ++saved;
        } else {
            qWarning() << "Failed to save" << path;
        }
    }

    // Legacy filenames used by older docs (symlink via duplicate save)
    const QString usbPath = out.filePath(QStringLiteral("usb-monitor.png"));
    const QString legacyMain = out.filePath(QStringLiteral("main-window.png"));
    const QString legacyIso = out.filePath(QStringLiteral("iso-verify-report.png"));
    if (QFile::exists(usbPath)) {
        QFile::remove(legacyMain);
        if (QFile::copy(usbPath, legacyMain)) {
            ++saved;
        }
    }
    const QString isoPath = out.filePath(QStringLiteral("iso-verifier.png"));
    if (QFile::exists(isoPath)) {
        QFile::remove(legacyIso);
        if (QFile::copy(isoPath, legacyIso)) {
            ++saved;
        }
    }
    const QString settingsPath = out.filePath(QStringLiteral("settings.png"));
    const QString legacyWatch = out.filePath(QStringLiteral("watch-lists.png"));
    if (QFile::exists(settingsPath)) {
        QFile::remove(legacyWatch);
        if (QFile::copy(settingsPath, legacyWatch)) {
            ++saved;
        }
    }

    qInfo() << "Captured" << shots.size() << "screenshots in" << out.absolutePath();
    return saved >= static_cast<int>(shots.size()) ? 0 : 1;
}

} // namespace FlashSpartan
