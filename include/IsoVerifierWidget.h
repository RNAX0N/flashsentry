#pragma once

#include "Types.h"

#include <QWidget>

class QComboBox;
class QFrame;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTableWidget;
class QLabel;
class QProgressBar;
class QTextEdit;
class QSplitter;
class QStackedWidget;

namespace FlashSpartan {

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
    void setAutoVerifyOnScan(bool enabled);
    void verifyMountPoint(const QString& mountPoint, const QString& deviceNode,
                          const QString& deviceLabel = {});

    void refreshCatalogStatus();
    void setActiveProfile(const QString& profileId);

    /** Switch to results view; re-run or show last report for this mount. */
    void focusDevice(const QString& deviceNode, const QString& mountPoint,
                     const QString& deviceLabel = {});

signals:
    void logMessageRequested(const QString& message);
    void verificationReportReady(const QString& deviceNode, const QList<IsoVerifyResult>& results);
    void settingsProfileSelected(const QString& profileId);

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
    void onProfileChanged(int index);
    void onTableCellClicked(int row, int column);
    void onVerificationFinished(const QString& location, const QString& deviceNode,
                                const QList<IsoVerifyResult>& results);

private:
    QWidget* buildEmptyStatePage();
    QWidget* buildMainPage();
    void setResults(const QList<IsoVerifyResult>& results);
    void appendReport(const QString& text);
    void updateCatalogIntegrityBanner();
    void updateMultibootBadge();
    void updateSummaryStrip(int passed, int total, int notVerified);
    void updateResultHint(const QList<IsoVerifyResult>& results);
    void updatePageVisibility();
    void scrollReportToRow(int row);
    void applyChromeStyles();
    void styleResultRow(int row, bool passed, bool inconclusive = false);
    void maybeAutoVerifyCurrentPath();

    IsoVerifierWorker* m_worker = nullptr;
    bool m_autoVerifyOnScan = true;
    QString m_lastAutoVerifiedPath;
    QStackedWidget* m_pageStack = nullptr;
    QLabel* m_catalogBanner = nullptr;
    QComboBox* m_profileCombo = nullptr;
    QLabel* m_introLabel = nullptr;
    QLabel* m_multibootBadge = nullptr;
    QFrame* m_summaryStrip = nullptr;
    QLabel* m_passChip = nullptr;
    QLabel* m_failChip = nullptr;
    QLabel* m_sidecarChip = nullptr;
    QLabel* m_legendLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
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

} // namespace FlashSpartan
