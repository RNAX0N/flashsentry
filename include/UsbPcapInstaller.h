#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace FlashSpartan {

/**
 * @brief Downloads and launches the official USBPcap installer (Windows), then polls until integrated.
 */
class UsbPcapInstaller : public QObject {
    Q_OBJECT

public:
    explicit UsbPcapInstaller(QObject* parent = nullptr);
    ~UsbPcapInstaller() override;

    static QString installPageUrl();
    static bool isInstalled(QString* executablePath = nullptr);

    /** Open https://desowin.org/usbpcap/ in the default browser. */
    static void openInstallPage();

    bool isWaitingForInstall() const { return m_waitingForInstall; }

public slots:
    /** Download official USBPcapSetup.exe (if needed) and start the installer UI. */
    void startDownloadAndInstall();

    /** Re-check for USBPcapCMD.exe and emit stateChanged. */
    void refreshInstallState();

    void stopPolling();

signals:
    void stateChanged(bool installed, const QString& statusMessage, const QString& executablePath);
    void downloadProgress(qint64 received, qint64 total);
    void installFlowMessage(const QString& message);
    void installFlowFailed(const QString& error);

private slots:
    void onPollTimer();

private:
    void beginPolling();
    void saveInstallerAndLaunch(QNetworkReply* reply);
    QString cachedInstallerPath() const;

    QNetworkAccessManager* m_network = nullptr;
    QTimer* m_pollTimer = nullptr;
    bool m_waitingForInstall = false;
    bool m_downloadInProgress = false;
};

} // namespace FlashSpartan
