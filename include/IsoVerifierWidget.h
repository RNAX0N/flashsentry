#pragma once

#include "Types.h"

#include <QWidget>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QLabel;
class QProgressBar;
class QTextEdit;

namespace FlashSentry {

class IsoVerifierWorker;

/**
 * @brief Automated ISO verification UI — scans USB mounts and publisher sites.
 */
class IsoVerifierWidget : public QWidget {
    Q_OBJECT

public:
    explicit IsoVerifierWidget(QWidget* parent = nullptr);
    ~IsoVerifierWidget() override;

    void setScanDirectory(const QString& path);
    void verifyMountPoint(const QString& mountPoint, const QString& deviceNode,
                          const QString& deviceLabel = {});

signals:
    void logMessageRequested(const QString& message);
    void verificationReportReady(const QString& deviceNode, const QList<IsoVerifyResult>& results);

public slots:
    void onUsbMountForIsoVerify(const QString& mountPoint, const QString& deviceNode,
                                const QString& deviceLabel);

private slots:
    void onBrowse();
    void onVerifyMount();
    void onShowReport();
    void onVerificationFinished(const QString& location, const QString& deviceNode,
                                const QList<IsoVerifyResult>& results);

private:
    void setResults(const QList<IsoVerifyResult>& results);
    void appendReport(const QString& text);

    IsoVerifierWorker* m_worker = nullptr;
    QLineEdit* m_dirEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QTextEdit* m_reportView = nullptr;
    QList<IsoVerifyResult> m_lastResults;
    QString m_lastDeviceNode;
};

} // namespace FlashSentry
