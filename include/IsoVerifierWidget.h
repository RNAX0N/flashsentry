#pragma once

#include "Types.h"

#include <QWidget>

class QFrame;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTableWidget;
class QLabel;
class QProgressBar;
class QTextEdit;
class QSplitter;

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

    void refreshCatalogStatus();

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
    void onExportReport();
    void onCopyReport();
    void onUpdateCatalog();
    void onPasteTrustedHash();
    void onScanPathEdited();
    void onVerificationFinished(const QString& location, const QString& deviceNode,
                                const QList<IsoVerifyResult>& results);

private:
    void setResults(const QList<IsoVerifyResult>& results);
    void appendReport(const QString& text);
    void updateCatalogIntegrityBanner();
    void updateMultibootBadge();
    void updateSummaryStrip(int passed, int total, int needsSidecar);
    void applyChromeStyles();
    void styleResultRow(int row, bool passed);

    IsoVerifierWorker* m_worker = nullptr;
    QLabel* m_catalogBanner = nullptr;
    QLabel* m_introLabel = nullptr;
    QLabel* m_multibootBadge = nullptr;
    QFrame* m_summaryStrip = nullptr;
    QLabel* m_passChip = nullptr;
    QLabel* m_failChip = nullptr;
    QLabel* m_sidecarChip = nullptr;
    QLineEdit* m_dirEdit = nullptr;
    QPushButton* m_verifyBtn = nullptr;
    QToolButton* m_toolsBtn = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QSplitter* m_splitter = nullptr;
    QTextEdit* m_reportView = nullptr;
    QList<IsoVerifyResult> m_lastResults;
    QString m_lastDeviceNode;
};

} // namespace FlashSentry
