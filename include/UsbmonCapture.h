#pragma once

#include "Types.h"

#include <QObject>
#include <QProcess>

namespace FlashSpartan {

class UsbmonCapture : public QObject {
    Q_OBJECT

public:
    explicit UsbmonCapture(QObject* parent = nullptr);

    QString outputDirectory() const;
    bool isRunning() const;

    bool startCapture(const HidDeviceInfo& device, const BadUsbAnomalyResult& anomaly,
                      const QString& commandTemplate);
    void stopCapture();

signals:
    void captureStarted(const QString& path);
    void captureFinished(const QString& path, int exitCode);
    void captureFailed(const QString& error);

private:
    QProcess* m_process = nullptr;
    QString m_outputPath;
};

} // namespace FlashSpartan
