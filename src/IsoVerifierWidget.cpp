#include "IsoVerifierWidget.h"
#include "IsoVerifierWorker.h"
#include "IsoVerifier.h"
#include "IsoCatalogManifest.h"
#include "IsoVerifyReport.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QFile>
#include <QTextEdit>
#include <QMessageBox>
#include <QScrollBar>
#include <QClipboard>
#include <QGuiApplication>
#include <QInputDialog>

namespace FlashSentry {

IsoVerifierWidget::IsoVerifierWidget(QWidget* parent)
    : QWidget(parent)
    , m_worker(new IsoVerifierWorker(this))
{
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(
        QStringLiteral("<b>Fully automated ISO verification</b> — built for people who want confidence without learning hashes or PGP.<br>"
                       "Plug in a stick with a copied <code>.iso</code> or <code>.img.xz</code> (Rufus, Ventoy, etc.): we detect it, fetch the publisher checksums, "
                       "verify signatures and key fingerprints for you — no Kleopatra or terminal commands.")));

    auto* dirRow = new QHBoxLayout;
    m_dirEdit = new QLineEdit;
    m_dirEdit->setPlaceholderText(QStringLiteral("Mount point or folder (auto-filled when a USB drive mounts)"));
    auto* browseBtn = new QPushButton(QStringLiteral("Browse…"));
    connect(browseBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onBrowse);
    dirRow->addWidget(m_dirEdit, 1);
    dirRow->addWidget(browseBtn);
    layout->addLayout(dirRow);

    auto* btnRow = new QHBoxLayout;
    auto* verifyBtn = new QPushButton(QStringLiteral("Verify now"));
    connect(verifyBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onVerifyMount);
    auto* reportBtn = new QPushButton(QStringLiteral("Full report"));
    connect(reportBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onShowReport);
    auto* exportBtn = new QPushButton(QStringLiteral("Export…"));
    connect(exportBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onExportReport);
    auto* copyBtn = new QPushButton(QStringLiteral("Copy report"));
    connect(copyBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onCopyReport);
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    connect(cancelBtn, &QPushButton::clicked, m_worker, &IsoVerifierWorker::cancel);
    auto* catalogBtn = new QPushButton(QStringLiteral("Update catalog"));
    connect(catalogBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onUpdateCatalog);
    auto* pasteHashBtn = new QPushButton(QStringLiteral("Trust hash…"));
    connect(pasteHashBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onPasteTrustedHash);
    btnRow->addWidget(verifyBtn);
    btnRow->addWidget(reportBtn);
    btnRow->addWidget(exportBtn);
    btnRow->addWidget(copyBtn);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(catalogBtn);
    btnRow->addWidget(pasteHashBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    m_progress = new QProgressBar;
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("ISO / note"),
        QStringLiteral("Publisher"),
        QStringLiteral("SHA-256"),
        QStringLiteral("PGP"),
        QStringLiteral("Fingerprint"),
        QStringLiteral("Status"),
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    m_summaryLabel = new QLabel(QStringLiteral("Waiting for USB mount or choose a folder."));
    layout->addWidget(m_summaryLabel);

    m_reportView = new QTextEdit;
    m_reportView->setReadOnly(true);
    m_reportView->setMaximumHeight(180);
    m_reportView->setPlaceholderText(QStringLiteral("Detailed verification report appears here…"));
    layout->addWidget(m_reportView);

    connect(m_worker, &IsoVerifierWorker::verificationStarted, this, [this]() {
        m_progress->setVisible(true);
        m_progress->setRange(0, 0);
        m_progress->setValue(0);
    });
    connect(m_worker, &IsoVerifierWorker::verificationFileProgress, this,
            [this](int current, int total, const QString& file) {
                if (total > 0) {
                    m_progress->setRange(0, total);
                    m_progress->setValue(current);
                }
                const QString msg =
                    QStringLiteral("Verifying %1 (%2/%3)…").arg(file).arg(current).arg(total);
                m_summaryLabel->setText(msg);
                emit logMessageRequested(msg);
            });
    connect(m_worker, &IsoVerifierWorker::verificationProgress, this, [this](const QString& msg) {
        m_summaryLabel->setText(msg);
        emit logMessageRequested(msg);
    });
    connect(m_worker, &IsoVerifierWorker::verificationFinished, this,
            &IsoVerifierWidget::onVerificationFinished);
    connect(m_worker, &IsoVerifierWorker::verificationFailed, this,
            [this](const QString& mount, const QString& err) {
                m_progress->setVisible(false);
                m_summaryLabel->setText(QStringLiteral("Verification failed: %1").arg(err));
                emit logMessageRequested(QStringLiteral("ISO verify failed (%1): %2").arg(mount, err));
            });
}

IsoVerifierWidget::~IsoVerifierWidget() = default;

void IsoVerifierWidget::setScanDirectory(const QString& path)
{
    m_dirEdit->setText(path);
}

void IsoVerifierWidget::verifyMountPoint(const QString& mountPoint, const QString& deviceNode,
                                         const QString& deviceLabel)
{
    if (mountPoint.isEmpty()) return;
    m_dirEdit->setText(mountPoint);
    m_lastDeviceNode = deviceNode;
    const QString label = deviceLabel.isEmpty() ? mountPoint : deviceLabel;
    m_summaryLabel->setText(QStringLiteral("Verifying ISOs on %1…").arg(label));
    m_worker->verifyMountPoint(mountPoint, deviceNode);
}

void IsoVerifierWidget::onUsbMountForIsoVerify(const QString& mountPoint, const QString& deviceNode,
                                               const QString& deviceLabel)
{
    verifyMountPoint(mountPoint, deviceNode, deviceLabel);
}

void IsoVerifierWidget::onBrowse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("ISO folder or mount point"));
    if (!dir.isEmpty()) {
        m_dirEdit->setText(dir);
    }
}

void IsoVerifierWidget::onVerifyMount()
{
    const QString path = m_dirEdit->text().trimmed();
    if (path.isEmpty()) {
        m_summaryLabel->setText(QStringLiteral("Enter a mount point or folder."));
        return;
    }
    m_worker->verifyDirectory(path, m_lastDeviceNode);
}

void IsoVerifierWidget::onShowReport()
{
    if (m_lastResults.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No report"),
                                 QStringLiteral("Run verification first."));
        return;
    }
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("ISO verification report"));
    box.setTextFormat(Qt::PlainText);
    box.setText(m_reportView->toPlainText());
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void IsoVerifierWidget::onExportReport()
{
    if (m_lastResults.isEmpty()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export report"), QStringLiteral("flashsentry-report.html"),
        QStringLiteral("HTML (*.html);;CSV (*.csv);;Text (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QString body;
    if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
        body = IsoVerifyReport::buildCsv(m_lastResults);
    } else if (path.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)) {
        body = IsoVerifyReport::buildHtml(m_lastResults);
    } else {
        body = IsoVerifyReport::buildPlainText(m_lastResults);
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(body.toUtf8());
        m_summaryLabel->setText(QStringLiteral("Report exported to %1").arg(path));
    }
}

void IsoVerifierWidget::onCopyReport()
{
    if (auto* clip = QGuiApplication::clipboard()) {
        clip->setText(m_reportView->toPlainText());
        m_summaryLabel->setText(QStringLiteral("Report copied to clipboard"));
    }
}

void IsoVerifierWidget::onUpdateCatalog()
{
    m_summaryLabel->setText(QStringLiteral("Updating ISO catalog…"));
    const bool ok = IsoCatalogManifest::refreshRemoteIfStale(0, true);
    m_summaryLabel->setText(ok ? QStringLiteral("Catalog updated (%1 entries)")
                                         .arg(IsoCatalogManifest::entryCount())
                               : QStringLiteral("Catalog update failed (using embedded copy)"));
}

void IsoVerifierWidget::onPasteTrustedHash()
{
    const QString file = QInputDialog::getText(this, QStringLiteral("Trust image hash"),
                                               QStringLiteral("Exact filename (e.g. Win11_24H2_English_x64.iso)"));
    if (file.isEmpty()) {
        return;
    }
    const QString hash = QInputDialog::getText(this, QStringLiteral("SHA-256 hex"),
                                               QStringLiteral("64-character SHA-256"));
    if (IsoCatalogManifest::trustUserHash(file, hash)) {
        m_summaryLabel->setText(QStringLiteral("Saved trusted hash for %1").arg(file));
    } else {
        QMessageBox::warning(this, QStringLiteral("Invalid hash"),
                             QStringLiteral("Expected a 64-character SHA-256 hex string."));
    }
}

void IsoVerifierWidget::onVerificationFinished(const QString& location, const QString& deviceNode,
                                               const QList<IsoVerifyResult>& results)
{
    Q_UNUSED(location)
    m_lastDeviceNode = deviceNode;
    setResults(results);
    m_progress->setVisible(false);
    emit verificationReportReady(deviceNode, results);
}

void IsoVerifierWidget::setResults(const QList<IsoVerifyResult>& results)
{
    m_lastResults = results;
    m_table->setRowCount(results.size());
    int ok = 0;
    m_reportView->clear();

    for (int i = 0; i < results.size(); ++i) {
        const IsoVerifyResult& r = results.at(i);
        const QString name = r.isoPath.isEmpty() ? r.layoutNote.left(80) : QFileInfo(r.isoPath).fileName();
        m_table->setItem(i, 0, new QTableWidgetItem(name));
        m_table->setItem(i, 1, new QTableWidgetItem(
            r.publisherName.isEmpty() ? QStringLiteral("—") : r.publisherName + QLatin1Char(' ') + r.releaseLabel));

        QString hashCol = r.hashChecked
                              ? (r.expectedSha256.isEmpty()
                                     ? QStringLiteral("computed")
                                     : (r.hashMatches ? QStringLiteral("OK") : QStringLiteral("MISMATCH")))
                              : QStringLiteral("—");
        m_table->setItem(i, 2, new QTableWidgetItem(hashCol));

        QString pgp = QStringLiteral("—");
        if (r.pgpChecked) {
            pgp = r.pgpValid ? QStringLiteral("valid") : QStringLiteral("FAIL");
        }
        m_table->setItem(i, 3, new QTableWidgetItem(pgp));

        QString fp = QStringLiteral("—");
        if (r.pgpChecked) {
            fp = r.fingerprintTrusted ? QStringLiteral("trusted") : QStringLiteral("unknown");
        }
        m_table->setItem(i, 4, new QTableWidgetItem(fp));

        const bool pass = r.passed();
        if (pass) ++ok;
        QString status = pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
        if (!r.errorMessage.isEmpty()) status += QLatin1String(": ") + r.errorMessage.left(60);
        m_table->setItem(i, 5, new QTableWidgetItem(status));

        appendReport(r.reportSummary);
        if (!r.reportSummary.isEmpty()) appendReport(QString());
    }

    m_summaryLabel->setText(QStringLiteral("%1 / %2 verification(s) passed.")
                                .arg(ok)
                                .arg(results.size()));
    emit logMessageRequested(QStringLiteral("ISO verify: %1/%2 passed").arg(ok).arg(results.size()));
}

void IsoVerifierWidget::appendReport(const QString& text)
{
    if (text.isEmpty()) {
        m_reportView->append(QString());
        return;
    }
    m_reportView->append(text);
    m_reportView->verticalScrollBar()->setValue(m_reportView->verticalScrollBar()->maximum());
}

} // namespace FlashSentry
