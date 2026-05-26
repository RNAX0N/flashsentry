#include "VerifyCli.h"

#include "IsoCatalogManifest.h"
#include "IsoVerifier.h"
#include "IsoVerifyReport.h"

#include <QFile>
#include <QFileInfo>
#include <iostream>

namespace FlashSentry {

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
    IsoCatalogManifest::ensureLoaded();
    const IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(path);
    printResults({r});
    return r.passed() ? VerifyCli::ExitOk : VerifyCli::ExitVerifyFailed;
}

int VerifyCli::runVerifyMount(const QString& mountPoint)
{
    IsoCatalogManifest::ensureLoaded();
    IsoCatalogManifest::refreshRemoteIfStale();
    const QList<IsoVerifyResult> results = IsoVerifier::verifyMountPoint(mountPoint);
    printResults(results);
    return resultsToExitCode(results);
}

int VerifyCli::runVerifyDir(const QString& directory)
{
    IsoCatalogManifest::ensureLoaded();
    const QList<IsoVerifyResult> results = IsoVerifier::verifyDirectory(directory);
    printResults(results);
    return resultsToExitCode(results);
}

int VerifyCli::runUpdateCatalog(bool force)
{
    Q_UNUSED(force)
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

} // namespace FlashSentry
