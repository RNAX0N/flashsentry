#include "VerifyCli.h"

#include "IsoCatalog.h"
#include "IsoCatalogManifest.h"
#include "IsoVerifier.h"
#include "IsoVerifyReport.h"
#include "IsoVerifySettingsLoader.h"

#include <QFile>
#include <QFileInfo>
#include <iostream>

namespace FlashSentry {

static QString s_configFilePath;

void VerifyCli::setConfigFilePath(const QString& path)
{
    s_configFilePath = path;
}

void VerifyCli::applyUserSettings()
{
    IsoVerifySettingsLoader::applyToVerifier(s_configFilePath);
}

static int resultsToExitCode(const QList<IsoVerifyResult>& results)
{
    if (results.isEmpty()) {
        std::cerr << "No images found to verify.\n";
        return VerifyCli::ExitError;
    }
    for (const IsoVerifyResult& r : results) {
        if (!r.passed()) {
            return VerifyCli::ExitVerifyFailed;
        }
    }
    return VerifyCli::ExitOk;
}

static void printResults(const QList<IsoVerifyResult>& results)
{
    std::cout << IsoVerifyReport::buildPlainText(results).toStdString();
}

int VerifyCli::runVerifyIso(const QString& path)
{
    applyUserSettings();
    IsoCatalogManifest::ensureLoaded();
    const IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(path);
    printResults({r});
    return r.passed() ? VerifyCli::ExitOk : VerifyCli::ExitVerifyFailed;
}

int VerifyCli::runVerifyMount(const QString& mountPoint)
{
    applyUserSettings();
    IsoCatalogManifest::ensureLoaded();
    IsoCatalogManifest::refreshRemoteIfStale();
    const QList<IsoVerifyResult> results = IsoVerifier::verifyMountPoint(mountPoint);
    printResults(results);
    return resultsToExitCode(results);
}

int VerifyCli::runVerifyDir(const QString& directory)
{
    applyUserSettings();
    IsoCatalogManifest::ensureLoaded();
    const QList<IsoVerifyResult> results = IsoVerifier::verifyDirectory(directory);
    printResults(results);
    return resultsToExitCode(results);
}

int VerifyCli::runUpdateCatalog(bool force)
{
    IsoCatalogManifest::ensureLoaded();
    const bool ok = IsoCatalogManifest::refreshRemoteIfStale(force ? 0 : 7 * 24 * 3600);
    if (ok) {
        std::cout << "ISO catalog updated (" << IsoCatalogManifest::entryCount() << " entries).\n";
        return VerifyCli::ExitOk;
    }
    std::cerr << "Failed to refresh ISO catalog manifest.\n";
    return VerifyCli::ExitError;
}

int VerifyCli::runExportReport(const QString& path, const QString& format)
{
    applyUserSettings();
    IsoCatalogManifest::ensureLoaded();
    QList<IsoVerifyResult> results;
    if (QFileInfo(path).isDir()) {
        results = IsoVerifier::verifyDirectory(path);
    } else {
        results.append(IsoVerifier::verifyIsoAutomated(path));
    }

    QString content;
    const QString fmt = format.toLower();
    if (fmt == QStringLiteral("html")) {
        content = IsoVerifyReport::buildHtml(results);
    } else if (fmt == QStringLiteral("csv")) {
        content = IsoVerifyReport::buildCsv(results);
    } else {
        content = IsoVerifyReport::buildPlainText(results);
    }

    std::cout << content.toStdString();
    return resultsToExitCode(results);
}

int VerifyCli::runListPublishers()
{
    IsoCatalogManifest::ensureLoaded();
    const QStringList ids = IsoCatalog::knownPublisherIds();
    std::cout << "Built-in publisher IDs (" << ids.size() << "):\n";
    for (const QString& id : ids) {
        std::cout << "  " << id.toStdString() << '\n';
    }
    std::cout << "Manifest entries: " << IsoCatalogManifest::entryCount() << '\n';
    if (!IsoCatalogManifest::lastEmbeddedIntegrityOk()) {
        std::cerr << "Warning: embedded manifest SHA-256 integrity check failed.\n";
        return ExitError;
    }
    return ExitOk;
}

int VerifyCli::runTrustHash(const QString& fileName, const QString& sha256Hex)
{
    IsoCatalogManifest::ensureLoaded();
    const QString base = QFileInfo(fileName).fileName();
    if (base.isEmpty() || sha256Hex.size() != 64) {
        std::cerr << "Usage: trust-hash requires a filename and 64-character SHA-256 hex.\n";
        return ExitError;
    }
    if (!IsoCatalogManifest::trustUserHash(base, sha256Hex)) {
        std::cerr << "Failed to save trusted hash.\n";
        return ExitError;
    }
    std::cout << "Trusted hash saved for " << base.toStdString() << '\n';
    return ExitOk;
}

} // namespace FlashSentry
