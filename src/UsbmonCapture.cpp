#include "UsbmonCapture.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace FlashSentry {

UsbmonCapture::UsbmonCapture(QObject* parent)
    : QObject(parent)
{
}

QString UsbmonCapture::outputDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/badusb-captures");
}

bool UsbmonCapture::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

bool UsbmonCapture::startCapture(const HidDeviceInfo& device,
                                 const BadUsbAnomalyResult& anomaly,
                                 const QString& commandTemplate)
{
#ifdef Q_OS_WIN
    Q_UNUSED(device)
    Q_UNUSED(anomaly)
    Q_UNUSED(commandTemplate)
    emit captureFailed(QStringLiteral(
        "USB packet capture is not implemented on Windows yet. Use USBPcap/Wireshark manually."));
    return false;
#else
    if (isRunning()) {
        emit captureFailed(QStringLiteral("A usbmon capture is already running"));
        return false;
    }

    QString bus = device.usbBus;
    bus.remove(QRegularExpression(QStringLiteral("^0+")));
    if (bus.isEmpty()) {
        bus = QStringLiteral("0");
    }

    QDir().mkpath(outputDirectory());
    const QString safeRule = anomaly.ruleId.isEmpty()
        ? QStringLiteral("manual")
        : QString(anomaly.ruleId).replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")),
                                          QStringLiteral("_"));
    m_outputPath = outputDirectory() + QLatin1Char('/')
                   + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss"))
                   + QLatin1Char('-') + safeRule + QStringLiteral(".pcap");

    QString templ = commandTemplate.trimmed();
    if (templ.isEmpty()) {
        templ = QStringLiteral("tcpdump -i usbmon{bus} -w {out} -G 30 -W 1");
    }
    templ.replace(QStringLiteral("{bus}"), bus);
    templ.replace(QStringLiteral("{out}"), m_outputPath);
    templ.replace(QStringLiteral("{stable_id}"), device.stableId());
    templ.replace(QStringLiteral("{rule_id}"), safeRule);

    const QStringList parts = QProcess::splitCommand(templ);
    if (parts.isEmpty()) {
        emit captureFailed(QStringLiteral("usbmon capture command is empty"));
        return false;
    }

    m_process = new QProcess(this);
    m_process->setProgram(parts.first());
    m_process->setArguments(parts.mid(1));
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit captureFailed(m_process ? m_process->errorString() : QStringLiteral("usbmon failed"));
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                const QString path = m_outputPath;
                QProcess* finished = m_process;
                m_process = nullptr;
                if (finished) {
                    finished->deleteLater();
                }
                emit captureFinished(path, exitCode);
            });

    m_process->start();
    if (!m_process->waitForStarted(3000)) {
        const QString error = m_process->errorString();
        m_process->deleteLater();
        m_process = nullptr;
        emit captureFailed(error);
        return false;
    }

    emit captureStarted(m_outputPath);
    return true;
#endif
}

void UsbmonCapture::stopCapture()
{
    if (!isRunning()) {
        return;
    }
    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
    }
}

} // namespace FlashSentry
