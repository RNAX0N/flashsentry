#include "MainWindow.h"
#include "DeviceVerificationPlanner.h"
#include "WatchListDialog.h"
#include "IsoVerifier.h"
#include "IsoVerifyReport.h"
#include "IsoVerifySettingsLoader.h"
#include "IsoCatalogManifest.h"
#include "IsoScanRules.h"
#include "SettingsProfiles.h"
#include "VerifyHistory.h"
#include <QMessageBox>

namespace FlashSpartan {

void MainWindow::applyAppModule()
{
    if (m_isoWidget) {
        if (!m_settings.isoScanDirectory.isEmpty()) {
            m_isoWidget->setScanDirectory(m_settings.isoScanDirectory);
        }
        m_isoWidget->refreshCatalogStatus();
    }
    Q_UNUSED(m_settings.appModule);
}

void MainWindow::onIsoLogMessage(const QString& message)
{
    logMessage(message, LogLevel::Info);
}

void MainWindow::startDeviceVerification(const QString& deviceNode)
{
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo) {
        return;
    }

    const QString deviceId = canonicalDeviceId(*deviceInfo);
    auto record = m_database->getDevice(deviceId);
    VerificationProfile profile = m_settings.defaultVerificationProfile;
    if (record) {
        profile = record->verificationProfile;
    }

    const auto plan = DeviceVerificationPlanner::planForDevice(
        profile, deviceInfo->mountPoint.isEmpty(), deviceInfo->isMounted);

    switch (plan.action) {
    case DeviceVerificationPlanner::StartAction::StartFullPartitionHash:
        promptAndStartHash(deviceNode, false);
        return;
    case DeviceVerificationPlanner::StartAction::MountThenVerify:
        m_pendingHashActions[deviceNode] = PendingHashAction::MountAfterVerify;
        if (!deviceInfo->isMounted) {
            m_mountManager->mount(deviceNode);
        }
        return;
    case DeviceVerificationPlanner::StartAction::StartManifestVerification:
        startManifestVerification(deviceNode);
        return;
    }
}

void MainWindow::startManifestVerification(const QString& deviceNode)
{
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo || deviceInfo->mountPoint.isEmpty()) {
        logMessage(QStringLiteral("Cannot verify manifest: %1 not mounted").arg(deviceNode),
                   LogLevel::Warning);
        return;
    }

    const QString deviceId = canonicalDeviceId(*deviceInfo);
    auto record = m_database->getDevice(deviceId);
    if (!record) {
        return;
    }

    if (!record->watchManifest.hasBaseline()) {
        openWatchListDialog(deviceNode);
        return;
    }

    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Hashing);
        card->setProgressVisible(true);
        card->setHashProgress(0);
    }

    const QString jobId = m_manifestWorker->startVerify(
        deviceNode, deviceInfo->mountPoint, deviceId, record->watchManifest);
    m_manifestJobDevices[jobId] = deviceNode;
}

void MainWindow::openWatchListDialog(const QString& deviceNode)
{
    auto deviceInfo = m_deviceMonitor->getDevice(deviceNode);
    if (!deviceInfo) {
        return;
    }
    if (deviceInfo->mountPoint.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Mount required"),
                               QStringLiteral("Mount the partition before editing watch lists."));
        m_mountManager->mount(deviceNode);
        return;
    }

    const QString deviceId = canonicalDeviceId(*deviceInfo);
    WatchManifest manifest;
    if (auto record = m_database->getDevice(deviceId)) {
        manifest = record->watchManifest;
    }

    WatchListDialog dialog(deviceInfo->mountPoint, deviceInfo->displayName(), manifest, this);
    connect(&dialog, &WatchListDialog::buildBaselineRequested, this,
            [this, deviceNode, deviceId](const WatchManifest& spec) {
                auto info = m_deviceMonitor->getDevice(deviceNode);
                if (!info) {
                    return;
                }
                const QString jobId = m_manifestWorker->startBuildBaseline(
                    deviceNode, info->mountPoint, deviceId, spec);
                m_manifestJobDevices[jobId] = deviceNode;
            });

    if (dialog.exec() == QDialog::Accepted) {
        WatchManifest updated = dialog.manifest();
        m_database->updateWatchManifest(deviceId, updated);
        m_database->setVerificationProfile(deviceId, dialog.selectedProfile());
        if (auto record = m_database->getDevice(deviceId)) {
            if (DeviceCard* card = getDeviceCard(deviceNode)) {
                card->setDeviceRecord(*record);
            }
        }
    }
}

void MainWindow::onWatchListRequested(const QString& deviceNode)
{
    openWatchListDialog(deviceNode);
}

void MainWindow::onManifestStarted(const QString& jobId, const QString& deviceNode)
{
    Q_UNUSED(jobId)
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Hashing);
        card->setProgressVisible(true);
    }
}

void MainWindow::onManifestCompleted(const QString& jobId, const ManifestVerifyResult& result)
{
    m_manifestJobDevices.remove(jobId);
    auto deviceInfo = m_deviceMonitor->getDevice(result.deviceNode);
    if (!deviceInfo) {
        return;
    }

    DeviceCard* card = getDeviceCard(result.deviceNode);
    if (card) {
        card->setProgressVisible(false);
    }

    if (result.matches) {
        logMessage(QStringLiteral("Watch manifest verified: %1").arg(deviceInfo->displayName()));
        {
            VerifyHistoryEntry he;
            he.deviceNode = result.deviceNode;
            he.deviceLabel = deviceInfo->displayName();
            he.mountPoint = deviceInfo->mountPoint;
            he.kind = VerifyHistoryKind::Manifest;
            he.status = QStringLiteral("pass");
            he.summary = QStringLiteral("Watch baseline OK");
            he.durationMs = result.durationMs;
            recordVerifyHistory(he);
        }
        if (card) {
            card->setVerificationStatus(VerificationStatus::Verified);
        }
        m_trayIcon->notifyVerificationResult(deviceInfo->displayName(), VerificationStatus::Verified);

        const QString deviceId = canonicalDeviceId(*deviceInfo);
        if (auto record = m_database->getDevice(deviceId)) {
            if (record->verificationProfile == VerificationProfile::Hybrid) {
                m_pendingHashActions[result.deviceNode] = PendingHashAction::RunFullHashAfterManifest;
                startHashing(result.deviceNode);
            }
        }

        const PendingHashAction pending = m_pendingHashActions.take(result.deviceNode);
        if (pending == PendingHashAction::MountAfterVerify && !deviceInfo->isMounted) {
            m_mountManager->mount(result.deviceNode);
        }
    } else {
        m_lastManifestResults[result.deviceNode] = result;
        {
            VerifyHistoryEntry he;
            he.deviceNode = result.deviceNode;
            he.deviceLabel = deviceInfo->displayName();
            he.mountPoint = deviceInfo->mountPoint;
            he.kind = VerifyHistoryKind::Manifest;
            he.status = QStringLiteral("mismatch");
            he.summary = result.changedPaths.isEmpty() ? result.addedPaths.join(QStringLiteral(", "))
                                                       : result.changedPaths.join(QStringLiteral(", "));
            he.durationMs = result.durationMs;
            recordVerifyHistory(he);
        }
        handleManifestMismatch(*deviceInfo, result);
    }
}

void MainWindow::onManifestBaselineBuilt(const QString& jobId, const QString& deviceId,
                                         const WatchManifest& manifest)
{
    const QString deviceNode = m_manifestJobDevices.take(jobId);
    m_database->updateWatchManifest(deviceId, manifest);

    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setProgressVisible(false);
        card->setVerificationStatus(VerificationStatus::Verified);
    }
    logMessage(QStringLiteral("Watch baseline saved (%1 group(s))").arg(manifest.groups.size()));

    if (!deviceNode.isEmpty()) {
        startManifestVerification(deviceNode);
    }
}

void MainWindow::onManifestFailed(const QString& jobId, const QString& error)
{
    const QString deviceNode = m_manifestJobDevices.take(jobId);
    logMessage(QStringLiteral("Manifest verify failed: %1").arg(error), LogLevel::Error);
    DeviceCard* card = getDeviceCard(deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Error);
        card->setProgressVisible(false);
    }
}

void MainWindow::handleManifestMismatch(const DeviceInfo& device, const ManifestVerifyResult& result)
{
    logMessage(QStringLiteral("Watch manifest mismatch: %1").arg(device.displayName()),
               LogLevel::Security);

    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Modified);
    }

    QString detail = result.changedPaths.join(QStringLiteral(", "));
    if (detail.isEmpty()) {
        detail = result.addedPaths.join(QStringLiteral(", "));
    }
    m_lastVerificationHashes[device.deviceNode] = result.computedRootHex;

    if (m_settings.requireConfirmationForModified) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("Watch list mismatch"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(QStringLiteral(
            "<b>%1</b> — watched files differ from the saved Merkle baseline.<br><br>%2")
                           .arg(device.displayName(), detail.toHtmlEscaped()));
        auto* approve = msgBox.addButton(QStringLiteral("Rebuild baseline"), QMessageBox::AcceptRole);
        auto* mountBtn = msgBox.addButton(QStringLiteral("Mount anyway"), QMessageBox::DestructiveRole);
        msgBox.addButton(QMessageBox::Cancel);
        msgBox.exec();
        if (msgBox.clickedButton() == approve) {
            openWatchListDialog(device.deviceNode);
        } else if (mountBtn && msgBox.clickedButton() == mountBtn) {
            mountDespiteModification(device);
        }
    }

    m_trayIcon->notifyVerificationResult(device.displayName(), VerificationStatus::Modified);
}

void MainWindow::acceptManifestBaseline(const DeviceInfo& device, const WatchManifest& manifest)
{
    const QString deviceId = canonicalDeviceId(device);
    m_database->updateWatchManifest(deviceId, manifest);
    DeviceCard* card = getDeviceCard(device.deviceNode);
    if (card) {
        card->setVerificationStatus(VerificationStatus::Verified);
    }
}

void MainWindow::applyIsoVerifyOptions()
{
    IsoVerifyOptions opt = IsoVerifySettingsLoader::load();
    opt.maxParallel = qMax(1, m_settings.isoVerifyParallel);
    opt.verifyDecompressed = m_settings.isoVerifyDecompressed;
    opt.preferOfflineSidecars = m_settings.isoPreferOfflineSidecars;
    IsoVerifier::setVerifyOptions(opt);
}

void MainWindow::maybeTriggerIsoVerifyForMountedDevice(const DeviceInfo& device)
{
    if (!m_isoWidget || device.mountPoint.isEmpty() || !device.isMounted) {
        return;
    }
    if (!m_settings.isoAutoVerifyOnUsbMount && m_settings.appModule != AppModule::IsoVerifier) {
        return;
    }
    if (m_isoVerifyTriggeredMounts.contains(device.mountPoint)) {
        return;
    }

    const IsoVerifier::MountScanResult scan = IsoVerifier::scanMountPoint(device.mountPoint);
    if (IsoScanRules::shouldSkipAutoVerifyPartition(device.mountPoint, device.sizeBytes,
                                                    scan.isoPaths.size())) {
        if (!scan.layoutNote.isEmpty()) {
            logMessage(scan.layoutNote, LogLevel::Info);
        }
        return;
    }
    if (scan.isoPaths.isEmpty() && !scan.looksLikeDdIsoStick) {
        return;
    }

    MountManager::MountResult synthetic;
    synthetic.success = true;
    synthetic.deviceNode = device.deviceNode;
    synthetic.mountPoint = device.mountPoint;
    triggerIsoVerificationOnMount(synthetic);
}

void MainWindow::clearIsoVerifyDedupForDevice(const DeviceInfo& device)
{
    if (!device.mountPoint.isEmpty()) {
        m_isoVerifyTriggeredMounts.remove(device.mountPoint);
    }
    for (const DeviceInfo& d : m_deviceMonitor->connectedDevices()) {
        if (d.parentDevice == device.parentDevice || d.deviceNode == device.parentDevice) {
            if (!d.mountPoint.isEmpty()) {
                m_isoVerifyTriggeredMounts.remove(d.mountPoint);
            }
        }
    }
}

void MainWindow::warnIfCatalogIntegrityFailed()
{
    if (IsoCatalogManifest::lastEmbeddedIntegrityOk()) {
        return;
    }
    const QString detail = IsoCatalogManifest::integrityStatusText();
    logMessage(QStringLiteral("Embedded ISO catalog integrity check failed — verification may be unreliable"),
               LogLevel::Security);
    QMessageBox::warning(
        this,
        QStringLiteral("ISO catalog integrity"),
        QStringLiteral(
            "%1\n\n"
            "Image verification may be unreliable until you reinstall FlashSpartan or run "
            "\"Update catalog\" from the ISO verification tab.\n\n"
            "If you did not modify system files, treat this as a possible installation problem.")
            .arg(detail));
    updateStatusBar();
}

void MainWindow::handleIsoVerificationReport(const QString& deviceNode,
                                             const QList<IsoVerifyResult>& results)
{
    const IsoVerifyReport::SummaryCounts counts = IsoVerifyReport::countSummary(results);
    const int passed = counts.passed;
    const int needsSidecar = counts.needsSidecar;
    const QString summary = IsoVerifyReport::summaryLine(results);
    logMessage(QStringLiteral("ISO verify (%1): %2")
                   .arg(deviceNode.isEmpty() ? QStringLiteral("manual") : deviceNode, summary),
               passed == results.size() ? LogLevel::Info : LogLevel::Security);

    if (m_settings.showNotifications && m_trayIcon) {
        auto info = m_deviceMonitor->getDevice(deviceNode);
        const QString name = info ? info->displayName() : deviceNode;
        m_trayIcon->notifyIsoVerifySummary(name, passed, results.size(), needsSidecar);
    }

    {
        VerifyHistoryEntry he;
        he.deviceNode = deviceNode;
        auto info = m_deviceMonitor->getDevice(deviceNode);
        he.deviceLabel = info ? info->displayName() : deviceNode;
        he.mountPoint = info ? info->mountPoint : QString();
        he.kind = VerifyHistoryKind::IsoScan;
        he.status = (passed == results.size() && !results.isEmpty())
                        ? QStringLiteral("pass")
                        : (needsSidecar > 0 ? QStringLiteral("partial") : QStringLiteral("fail"));
        he.summary = summary;
        recordVerifyHistory(he);
    }

    refreshVerifyHistoryPanel(deviceNode);

    if (DeviceCard* card = getDeviceCard(deviceNode)) {
        card->setIsoVerifySummary(summary);
        if (passed == results.size() && !results.isEmpty()) {
            card->setVerificationStatus(VerificationStatus::Verified);
        } else if (IsoVerifier::mountScanHasFailures(results)) {
            card->setVerificationStatus(VerificationStatus::Modified);
        }
    }

    if (m_settings.blockMountOnIsoVerifyFailure && IsoVerifier::mountScanHasFailures(results)) {
        logMessage(QStringLiteral("Mount blocked: ISO/image verification failed on %1").arg(deviceNode),
                   LogLevel::Security);
        m_pendingHashActions.remove(deviceNode);
    }
}

} // namespace FlashSpartan
