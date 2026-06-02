#include "UsbPcapInstaller.h"
#include "UsbPcapLocator.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace FlashSpartan {

namespace {

constexpr const char* kUsbPcapVersion = "1.5.4.0";

QStringList installerDownloadUrls()
{
    const QString fileName = QStringLiteral("USBPcapSetup-%1.exe").arg(QLatin1String(kUsbPcapVersion));
    return {
        QStringLiteral("https://desowin.org/usbpcap/") + fileName,
        QStringLiteral("https://github.com/desowin/USBpcap/releases/download/%1/%2")
            .arg(QLatin1String(kUsbPcapVersion), fileName),
    };
}

QString cacheDirectory()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/usbpcap");
    QDir().mkpath(base);
    return base;
}

} // namespace

QString UsbPcapInstaller::installPageUrl()
{
    return QStringLiteral("https://desowin.org/usbpcap/");
}

bool UsbPcapInstaller::isInstalled(QString* executablePath)
{
    const QString path = UsbPcapLocator::findUsbPcapCmdExecutable();
    if (path.isEmpty()) {
        return false;
    }
    if (executablePath) {
        *executablePath = path;
    }
    return true;
}

void UsbPcapInstaller::openInstallPage()
{
    QDesktopServices::openUrl(QUrl(installPageUrl()));
}

UsbPcapInstaller::UsbPcapInstaller(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &UsbPcapInstaller::onPollTimer);
}

UsbPcapInstaller::~UsbPcapInstaller()
{
    stopPolling();
}

QString UsbPcapInstaller::cachedInstallerPath() const
{
    return cacheDirectory() + QStringLiteral("/USBPcapSetup-") + QLatin1String(kUsbPcapVersion)
           + QStringLiteral(".exe");
}

void UsbPcapInstaller::refreshInstallState()
{
    QString path;
    const bool installed = isInstalled(&path);
    if (installed) {
        m_waitingForInstall = false;
        stopPolling();
    }

    QString status;
    if (installed) {
        status = QStringLiteral("USBPcap is installed and ready for packet capture.");
    } else if (m_waitingForInstall) {
        status = QStringLiteral(
            "Waiting for USBPcap installation to finish… FlashSpartan will detect it automatically.");
    } else {
#ifdef Q_OS_WIN
        status = QStringLiteral(
            "Packet capture requires USBPcap. Download and run the official installer, then return here.");
#else
        status = QStringLiteral("Packet capture requires usbmon and tcpdump on Linux.");
#endif
    }
    emit stateChanged(installed, status, path);
}

void UsbPcapInstaller::startDownloadAndInstall()
{
#ifndef Q_OS_WIN
    openInstallPage();
    emit installFlowMessage(
        QStringLiteral("USBPcap is a Windows driver. See the project wiki for Linux usbmon capture."));
    refreshInstallState();
    return;
#else
    if (isInstalled()) {
        refreshInstallState();
        emit installFlowMessage(QStringLiteral("USBPcap is already installed."));
        return;
    }

    const QString cached = cachedInstallerPath();
    if (QFileInfo::exists(cached) && QFileInfo(cached).size() > 100000) {
        emit installFlowMessage(QStringLiteral("Launching USBPcap installer…"));
        if (!QProcess::startDetached(cached, {})) {
            emit installFlowFailed(
                QStringLiteral("Could not start USBPcap installer. Try running as administrator or use "
                               "Open download page."));
            openInstallPage();
            return;
        }
        beginPolling();
        return;
    }

    if (m_downloadInProgress) {
        return;
    }

    m_downloadInProgress = true;
    m_waitingForInstall = true;
    emit installFlowMessage(QStringLiteral("Downloading USBPcap installer…"));

    const QStringList urls = installerDownloadUrls();
    if (urls.isEmpty()) {
        m_downloadInProgress = false;
        emit installFlowFailed(QStringLiteral("No download URL configured."));
        openInstallPage();
        return;
    }

    QNetworkRequest request{QUrl(urls.first())};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit downloadProgress(received, total); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, urls]() {
        m_downloadInProgress = false;

        if (reply->error() != QNetworkReply::NoError) {
            if (urls.size() > 1) {
                emit installFlowMessage(QStringLiteral("Trying alternate download mirror…"));
                QNetworkRequest fallback{QUrl(urls.at(1))};
                fallback.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                      QNetworkRequest::NoLessSafeRedirectPolicy);
                QNetworkReply* retry = m_network->get(fallback);
                connect(retry, &QNetworkReply::downloadProgress, this,
                        [this](qint64 received, qint64 total) {
                            emit downloadProgress(received, total);
                        });
                connect(retry, &QNetworkReply::finished, this, [this, retry]() {
                    if (retry->error() != QNetworkReply::NoError) {
                        emit installFlowFailed(
                            QStringLiteral("Download failed: %1").arg(retry->errorString()));
                        openInstallPage();
                        refreshInstallState();
                        retry->deleteLater();
                        return;
                    }
                    saveInstallerAndLaunch(retry);
                    retry->deleteLater();
                });
                return;
            }
            emit installFlowFailed(QStringLiteral("Download failed: %1").arg(reply->errorString()));
            openInstallPage();
            refreshInstallState();
            return;
        }
        saveInstallerAndLaunch(reply);
        reply->deleteLater();
    });
#endif
}

void UsbPcapInstaller::saveInstallerAndLaunch(QNetworkReply* reply)
{
#ifdef Q_OS_WIN
    if (!reply) {
        return;
    }

    const QByteArray data = reply->readAll();
    if (data.size() < 100000) {
        emit installFlowFailed(QStringLiteral("Downloaded file looks too small to be USBPcapSetup.exe."));
        openInstallPage();
        refreshInstallState();
        return;
    }

    const QString dest = cachedInstallerPath();
    QFile out(dest);
    if (!out.open(QIODevice::WriteOnly)) {
        emit installFlowFailed(QStringLiteral("Could not save installer to %1").arg(dest));
        openInstallPage();
        refreshInstallState();
        return;
    }
    out.write(data);
    out.close();

    emit installFlowMessage(QStringLiteral("Launching USBPcap installer — approve UAC if prompted…"));
    if (!QProcess::startDetached(dest, {})) {
        emit installFlowFailed(
            QStringLiteral("Could not launch installer. Open the download page and run it manually."));
        openInstallPage();
        refreshInstallState();
        return;
    }

    beginPolling();
#else
    Q_UNUSED(reply);
#endif
}

void UsbPcapInstaller::beginPolling()
{
    m_waitingForInstall = true;
    refreshInstallState();
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
}

void UsbPcapInstaller::stopPolling()
{
    m_pollTimer->stop();
    m_waitingForInstall = false;
}

void UsbPcapInstaller::onPollTimer()
{
    refreshInstallState();
    if (isInstalled()) {
        emit installFlowMessage(
            QStringLiteral("USBPcap detected — packet capture is ready."));
    }
}

} // namespace FlashSpartan
