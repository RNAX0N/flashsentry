#include "IsoVerifierWidget.h"
#include "IsoVerifierWorker.h"
#include "IsoVerifier.h"
#include "IsoCatalogManifest.h"
#include "IsoScanRules.h"
#include "IsoVerifyReport.h"
#include "SettingsProfiles.h"
#include "StyleManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
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
#include <QSplitter>
#include <QFrame>
#include <QMenu>
#include <QFileInfo>
#include <QStackedWidget>
#include <QComboBox>
#include <QTextCursor>

namespace FlashSentry {

namespace {

QLabel* makeChip(QWidget* parent)
{
    auto* chip = new QLabel(parent);
    chip->setAlignment(Qt::AlignCenter);
    chip->setMinimumHeight(28);
    chip->setVisible(false);
    return chip;
}

QString chipStyle(const QColor& bg, const QColor& fg)
{
    return QStringLiteral(
               "QLabel { background-color: %1; color: %2; padding: 4px 12px; border-radius: 14px; "
               "font-weight: 600; }")
        .arg(bg.name(QColor::HexArgb), fg.name(QColor::HexArgb));
}

} // namespace

IsoVerifierWidget::IsoVerifierWidget(QWidget* parent)
    : QWidget(parent)
    , m_worker(new IsoVerifierWorker(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    m_catalogBanner = new QLabel;
    m_catalogBanner->setWordWrap(true);
    m_catalogBanner->setVisible(false);
    layout->addWidget(m_catalogBanner);

    auto* profileRow = new QHBoxLayout;
    auto* profileLabel = new QLabel(QStringLiteral("Verification profile"));
    profileLabel->setFont(FSFont(Small));
    m_profileCombo = new QComboBox;
    for (const QString& id : SettingsProfiles::profileIds()) {
        m_profileCombo->addItem(SettingsProfiles::profileDisplayName(id), id);
    }
    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &IsoVerifierWidget::onProfileChanged);
    profileRow->addWidget(profileLabel);
    profileRow->addWidget(m_profileCombo, 1);
    layout->addLayout(profileRow);

    m_introLabel = new QLabel(
        QStringLiteral("Verify <code>.iso</code>, <code>.img.xz</code>, and related images on any USB "
                       "volume — whether you used <code>dd</code>, Rufus, a file copy, or another "
                       "tool. Publisher checksums and signatures are checked automatically."));
    m_introLabel->setWordWrap(true);
    m_introLabel->setObjectName(QStringLiteral("IsoIntroLabel"));
    layout->addWidget(m_introLabel);

    m_multibootBadge = new QLabel;
    m_multibootBadge->setWordWrap(true);
    m_multibootBadge->setVisible(false);
    m_multibootBadge->setObjectName(QStringLiteral("IsoMultibootBadge"));
    layout->addWidget(m_multibootBadge);

    m_summaryStrip = new QFrame;
    m_summaryStrip->setObjectName(QStringLiteral("IsoSummaryStrip"));
    auto* stripLayout = new QHBoxLayout(m_summaryStrip);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->setSpacing(8);
    m_passChip = makeChip(m_summaryStrip);
    m_failChip = makeChip(m_summaryStrip);
    m_sidecarChip = makeChip(m_summaryStrip);
    stripLayout->addWidget(m_passChip);
    stripLayout->addWidget(m_failChip);
    stripLayout->addWidget(m_sidecarChip);
    stripLayout->addStretch();
    m_summaryStrip->setVisible(false);
    layout->addWidget(m_summaryStrip);

    m_pageStack = new QStackedWidget;
    m_pageStack->addWidget(buildEmptyStatePage());
    m_pageStack->addWidget(buildMainPage());
    layout->addWidget(m_pageStack, 1);

    connect(m_worker, &IsoVerifierWorker::verificationStarted, this, [this]() {
        m_pageStack->setCurrentIndex(1);
        m_progress->setVisible(true);
        m_progress->setRange(0, 0);
        m_progress->setValue(0);
        m_verifyBtn->setEnabled(false);
        m_summaryStrip->setVisible(false);
    });
    connect(m_worker, &IsoVerifierWorker::verificationFileProgress, this,
            [this](int current, int total, const QString& file) {
                if (total > 0) {
                    m_progress->setRange(0, total);
                    m_progress->setValue(current);
                    m_progress->setFormat(QStringLiteral("%1 / %2 — %3").arg(current).arg(total).arg(
                        QFileInfo(file).fileName()));
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
                m_verifyBtn->setEnabled(true);
                m_summaryLabel->setText(QStringLiteral("Verification failed: %1").arg(err));
                emit logMessageRequested(QStringLiteral("ISO verify failed (%1): %2").arg(mount, err));
            });

    applyChromeStyles();
    updateCatalogIntegrityBanner();
    updateMultibootBadge();
    updatePageVisibility();
}

IsoVerifierWidget::~IsoVerifierWidget() = default;

QWidget* IsoVerifierWidget::buildEmptyStatePage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    auto* icon = new QLabel(QStringLiteral("💾"));
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral("font-size: 72px;"));
    layout->addWidget(icon);

    auto* title = new QLabel(QStringLiteral("Plug in a USB flash drive"));
    title->setFont(FSFont(Heading3));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* body = new QLabel(
        QStringLiteral("FlashSentry will verify image files on the mounted volume, or you can "
                       "pick a folder manually.\nWorks with any copy method — not tied to one "
                       "multiboot tool."));
    body->setWordWrap(true);
    body->setAlignment(Qt::AlignCenter);
    body->setMaximumWidth(480);
    body->setStyleSheet(QString("color: %1;")
                            .arg(FSStyle.colorCss(StyleManager::ColorRole::TextSecondary)));
    layout->addWidget(body);

    auto* browseBtn = new QPushButton(QStringLiteral("Choose folder or mount point…"));
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    connect(browseBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onBrowse);
    layout->addWidget(browseBtn, 0, Qt::AlignCenter);

    return page;
}

QWidget* IsoVerifierWidget::buildMainPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* pathGroup = new QFrame;
    pathGroup->setObjectName(QStringLiteral("IsoPathGroup"));
    auto* pathLayout = new QVBoxLayout(pathGroup);
    pathLayout->setContentsMargins(12, 10, 12, 10);
    pathLayout->setSpacing(8);

    auto* pathLabel = new QLabel(QStringLiteral("Folder or mount point"));
    pathLabel->setFont(FSFont(Small));
    pathLayout->addWidget(pathLabel);

    auto* dirRow = new QHBoxLayout;
    m_dirEdit = new QLineEdit;
    m_dirEdit->setPlaceholderText(
        QStringLiteral("/run/media/you/USB — filled when a removable drive mounts"));
    connect(m_dirEdit, &QLineEdit::textChanged, this, &IsoVerifierWidget::onScanPathEdited);
    auto* browseBtn = new QPushButton(QStringLiteral("Browse…"));
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onBrowse);
    dirRow->addWidget(m_dirEdit, 1);
    dirRow->addWidget(browseBtn);
    pathLayout->addLayout(dirRow);
    layout->addWidget(pathGroup);

    auto* actionRow = new QHBoxLayout;
    m_verifyBtn = new QPushButton(QStringLiteral("Verify images"));
    m_verifyBtn->setObjectName(QStringLiteral("PrimaryButton"));
    m_verifyBtn->setCursor(Qt::PointingHandCursor);
    m_verifyBtn->setMinimumHeight(36);
    m_verifyBtn->setStyleSheet(FSStyle.primaryButtonStyleSheet());
    connect(m_verifyBtn, &QPushButton::clicked, this, &IsoVerifierWidget::onVerifyMount);

    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, m_worker, &IsoVerifierWorker::cancel);

    m_toolsBtn = new QToolButton;
    m_toolsBtn->setText(QStringLiteral("More"));
    m_toolsBtn->setPopupMode(QToolButton::InstantPopup);
    m_toolsBtn->setCursor(Qt::PointingHandCursor);
    auto* toolsMenu = new QMenu(m_toolsBtn);
    toolsMenu->addAction(QStringLiteral("Full report…"), this, &IsoVerifierWidget::onShowReport);
    toolsMenu->addAction(QStringLiteral("Export report…"), this, &IsoVerifierWidget::onExportReport);
    toolsMenu->addAction(QStringLiteral("Copy report"), this, &IsoVerifierWidget::onCopyReport);
    toolsMenu->addSeparator();
    toolsMenu->addAction(QStringLiteral("Update catalog"), this, &IsoVerifierWidget::onUpdateCatalog);
    toolsMenu->addAction(QStringLiteral("Trust hash…"), this, &IsoVerifierWidget::onPasteTrustedHash);
    m_toolsBtn->setMenu(toolsMenu);

    actionRow->addWidget(m_verifyBtn);
    actionRow->addWidget(cancelBtn);
    actionRow->addWidget(m_toolsBtn);
    actionRow->addStretch();
    layout->addLayout(actionRow);

    m_progress = new QProgressBar;
    m_progress->setVisible(false);
    m_progress->setTextVisible(true);
    layout->addWidget(m_progress);

    m_summaryLabel = new QLabel(QStringLiteral("Ready to verify."));
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    m_splitter = new QSplitter(Qt::Vertical);
    m_splitter->setChildrenCollapsible(false);

    m_table = new QTableWidget(0, 6);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Image"),
        QStringLiteral("Publisher"),
        QStringLiteral("SHA-256"),
        QStringLiteral("PGP"),
        QStringLiteral("Key"),
        QStringLiteral("Result"),
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::cellClicked, this, &IsoVerifierWidget::onTableCellClicked);
    m_splitter->addWidget(m_table);

    m_reportView = new QTextEdit;
    m_reportView->setReadOnly(true);
    m_reportView->setPlaceholderText(
        QStringLiteral("Click a table row to jump to that image in the report…"));
    m_splitter->addWidget(m_reportView);
    m_splitter->setSizes({320, 160});

    layout->addWidget(m_splitter, 1);
    return page;
}

void IsoVerifierWidget::applyChromeStyles()
{
    const QString surface = FSStyle.colorCss(StyleManager::ColorRole::Surface);
    const QString border = FSStyle.colorCss(StyleManager::ColorRole::Border);
    const QString accent = FSStyle.colorCss(StyleManager::ColorRole::AccentPrimary);
    const QString text2 = FSStyle.colorCss(StyleManager::ColorRole::TextSecondary);

    setStyleSheet(QStringLiteral(
                      "#IsoPathGroup { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
                      "#IsoIntroLabel { color: %3; }"
                      "#IsoMultibootBadge { background-color: rgba(0, 180, 220, 0.12); color: %4; "
                      "padding: 8px 10px; border-radius: 6px; border: 1px solid %4; }"
                      "#PrimaryButton { font-weight: 600; min-width: 140px; }")
                      .arg(surface, border, text2, accent));

    if (m_dirEdit) {
        m_dirEdit->setStyleSheet(FSStyle.inputFieldStyleSheet());
    }
    if (m_table) {
        m_table->setStyleSheet(FSStyle.listWidgetStyleSheet());
    }
    if (m_reportView) {
        m_reportView->setStyleSheet(FSStyle.inputFieldStyleSheet());
    }
    if (m_progress) {
        m_progress->setStyleSheet(FSStyle.progressBarStyleSheet());
    }
    if (m_profileCombo) {
        m_profileCombo->setStyleSheet(FSStyle.inputFieldStyleSheet());
    }
}

void IsoVerifierWidget::setActiveProfile(const QString& profileId)
{
    if (!m_profileCombo) {
        return;
    }
    const QString id = SettingsProfiles::normalizeProfileId(profileId);
    const int idx = m_profileCombo->findData(id);
    if (idx >= 0) {
        m_profileCombo->blockSignals(true);
        m_profileCombo->setCurrentIndex(idx);
        m_profileCombo->blockSignals(false);
    }
}

void IsoVerifierWidget::onProfileChanged(int index)
{
    if (index < 0 || !m_profileCombo) {
        return;
    }
    emit settingsProfileSelected(m_profileCombo->itemData(index).toString());
}

void IsoVerifierWidget::refreshCatalogStatus()
{
    updateCatalogIntegrityBanner();
}

void IsoVerifierWidget::setScanDirectory(const QString& path)
{
    if (m_dirEdit) {
        m_dirEdit->setText(path);
    }
    updateMultibootBadge();
    updatePageVisibility();
}


void IsoVerifierWidget::focusDevice(const QString& deviceNode, const QString& mountPoint,
                                    const QString& deviceLabel)
{
    m_pageStack->setCurrentIndex(1);
    if (!deviceLabel.isEmpty()) {
        m_summaryLabel->setText(QStringLiteral("USB: %1").arg(deviceLabel));
    }
    if (!m_lastResults.isEmpty() && m_lastDeviceNode == deviceNode) {
        setResults(m_lastResults);
        m_summaryLabel->setText(QStringLiteral("Last ISO report for %1 — click a table row for details")
                                  .arg(deviceLabel.isEmpty() ? deviceNode : deviceLabel));
        return;
    }
    verifyMountPoint(mountPoint, deviceNode, deviceLabel);
}

void IsoVerifierWidget::verifyMountPoint(const QString& mountPoint, const QString& deviceNode,
                                         const QString& deviceLabel)
{
    if (mountPoint.isEmpty()) {
        return;
    }
    m_pageStack->setCurrentIndex(1);
    m_dirEdit->setText(mountPoint);
    m_lastDeviceNode = deviceNode;
    const QString label = deviceLabel.isEmpty() ? mountPoint : deviceLabel;
    m_summaryLabel->setText(QStringLiteral("Verifying images on %1…").arg(label));
    m_worker->verifyMountPoint(mountPoint, deviceNode);
    updateMultibootBadge();
    updatePageVisibility();
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
        m_pageStack->setCurrentIndex(1);
        updatePageVisibility();
    }
}

void IsoVerifierWidget::onScanPathEdited()
{
    updateMultibootBadge();
    updatePageVisibility();
}

void IsoVerifierWidget::updatePageVisibility()
{
    if (!m_pageStack) {
        return;
    }
    const bool hasPath = m_dirEdit && !m_dirEdit->text().trimmed().isEmpty();
    const bool hasResults = !m_lastResults.isEmpty();
    if (hasResults || hasPath) {
        m_pageStack->setCurrentIndex(1);
    } else {
        m_pageStack->setCurrentIndex(0);
    }
}

void IsoVerifierWidget::updateMultibootBadge()
{
    if (!m_dirEdit || !m_multibootBadge) {
        return;
    }
    const QString path = m_dirEdit->text().trimmed();
    if (path.isEmpty()) {
        m_multibootBadge->hide();
        return;
    }
    const MultibootLayout layout = IsoScanRules::detectMultibootLayout(path);
    if (layout.tool == MultibootTool::None && layout.summary.isEmpty()) {
        m_multibootBadge->hide();
        return;
    }
    QString text = layout.summary;
    const QString note = IsoScanRules::coexistenceNote(layout.tool);
    if (!note.isEmpty()) {
        text += QStringLiteral(" — ") + note;
    }
    m_multibootBadge->setText(text);
    m_multibootBadge->show();
}

void IsoVerifierWidget::updateSummaryStrip(int passed, int total, int needsSidecar)
{
    if (!m_summaryStrip) {
        return;
    }
    if (total <= 0) {
        m_summaryStrip->setVisible(false);
        return;
    }
    const int failed = total - passed - needsSidecar;
    m_passChip->setText(QStringLiteral("✓ %1 passed").arg(passed));
    m_passChip->setStyleSheet(chipStyle(FSColor(Verified).lighter(160), FSColor(Verified)));
    m_passChip->setVisible(passed > 0);

    m_failChip->setText(QStringLiteral("✗ %1 failed").arg(qMax(0, failed)));
    m_failChip->setStyleSheet(chipStyle(FSColor(Modified).lighter(170), FSColor(Modified)));
    m_failChip->setVisible(failed > 0);

    m_sidecarChip->setText(QStringLiteral("? %1 need sidecar").arg(needsSidecar));
    m_sidecarChip->setStyleSheet(chipStyle(FSColor(Warning).lighter(170), FSColor(Warning)));
    m_sidecarChip->setVisible(needsSidecar > 0);

    m_summaryStrip->setVisible(true);
}

void IsoVerifierWidget::styleResultRow(int row, bool passed)
{
    if (!m_table) {
        return;
    }
    const QColor bg = passed ? FSColor(Verified).lighter(190) : FSColor(Modified).lighter(195);
    for (int col = 0; col < m_table->columnCount(); ++col) {
        if (QTableWidgetItem* item = m_table->item(row, col)) {
            item->setBackground(bg);
        }
    }
}

void IsoVerifierWidget::scrollReportToRow(int row)
{
    if (!m_reportView || row < 0 || row >= m_lastResults.size()) {
        return;
    }
    const IsoVerifyResult& r = m_lastResults.at(row);
    QString needle;
    if (!r.isoPath.isEmpty()) {
        needle = QFileInfo(r.isoPath).fileName();
    } else if (!r.layoutNote.isEmpty()) {
        needle = r.layoutNote.left(40);
    }
    if (needle.isEmpty()) {
        return;
    }

    int idx = m_reportView->toPlainText().indexOf(needle);
    if (idx < 0 && !r.reportSummary.isEmpty()) {
        const QStringList lines = r.reportSummary.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (line.size() > 8) {
                idx = m_reportView->toPlainText().indexOf(line.left(24));
                if (idx >= 0) {
                    break;
                }
            }
        }
    }
    if (idx < 0) {
        return;
    }

    QTextCursor cursor(m_reportView->document());
    cursor.setPosition(idx);
    m_reportView->setTextCursor(cursor);
    m_reportView->ensureCursorVisible();
}

void IsoVerifierWidget::onTableCellClicked(int row, int /*column*/)
{
    if (!m_table) {
        return;
    }
    m_table->selectRow(row);
    scrollReportToRow(row);
}

void IsoVerifierWidget::onVerifyMount()
{
    const QString path = m_dirEdit->text().trimmed();
    if (path.isEmpty()) {
        m_summaryLabel->setText(QStringLiteral("Enter a mount point or folder."));
        return;
    }
    m_pageStack->setCurrentIndex(1);
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
        QStringLiteral("HTML (*.html);;JSON (*.json);;CSV (*.csv);;Text (*.txt)"));
    if (path.isEmpty()) {
        return;
    }
    QString body;
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        body = IsoVerifyReport::buildJson(m_lastResults);
    } else if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
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
    updateCatalogIntegrityBanner();
    m_summaryLabel->setText(ok ? QStringLiteral("Catalog updated (%1 entries)")
                                         .arg(IsoCatalogManifest::entryCount())
                               : QStringLiteral("Catalog update failed (using embedded copy)"));
}

void IsoVerifierWidget::updateCatalogIntegrityBanner()
{
    IsoCatalogManifest::ensureLoaded();
    if (!m_catalogBanner) {
        return;
    }
    if (IsoCatalogManifest::lastEmbeddedIntegrityOk()) {
        m_catalogBanner->setVisible(false);
        return;
    }
    m_catalogBanner->setText(
        QStringLiteral("<b>Catalog integrity warning</b> — %1 Use <b>More → Update catalog</b> "
                       "or reinstall if this persists.")
            .arg(IsoCatalogManifest::integrityStatusText()));
    m_catalogBanner->setStyleSheet(
        QStringLiteral("background-color: rgba(255, 180, 0, 0.15); color: #e6a700; padding: 10px; "
                       "border-radius: 6px; border: 1px solid #e6a700;"));
    m_catalogBanner->setVisible(true);
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
    m_verifyBtn->setEnabled(true);
    updatePageVisibility();
    emit verificationReportReady(deviceNode, results);
}

void IsoVerifierWidget::setResults(const QList<IsoVerifyResult>& results)
{
    m_lastResults = results;
    m_table->setRowCount(results.size());
    m_reportView->clear();

    const IsoVerifyReport::SummaryCounts counts = IsoVerifyReport::countSummary(results);

    for (int i = 0; i < results.size(); ++i) {
        const IsoVerifyResult& r = results.at(i);
        const QString name = r.isoPath.isEmpty() ? r.layoutNote.left(80) : QFileInfo(r.isoPath).fileName();
        m_table->setItem(i, 0, new QTableWidgetItem(name));
        m_table->setItem(i, 1, new QTableWidgetItem(
            r.publisherName.isEmpty() ? QStringLiteral("—")
                                      : r.publisherName + QLatin1Char(' ') + r.releaseLabel));

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
        QString status = pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
        if (!r.errorMessage.isEmpty()) {
            status += QLatin1String(": ") + r.errorMessage.left(60);
        }
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setFont(FSFont(Label));
        m_table->setItem(i, 5, statusItem);

        styleResultRow(i, pass);
        appendReport(r.reportSummary);
        if (!r.reportSummary.isEmpty()) {
            appendReport(QString());
        }
    }

    updateSummaryStrip(counts.passed, counts.total, counts.needsSidecar);
    m_summaryLabel->setText(IsoVerifyReport::summaryLine(results));
    emit logMessageRequested(QStringLiteral("ISO verify: %1").arg(IsoVerifyReport::summaryLine(results)));

    if (!results.isEmpty()) {
        m_table->selectRow(0);
        scrollReportToRow(0);
    }
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
