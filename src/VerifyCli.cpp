#include "VerifyCli.h"

#include "IsoCatalog.h"
#include "IsoCatalogManifest.h"
#include "IsoVerifier.h"
#include "IsoVerifyReport.h"
#include "IsoVerifySettingsLoader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

namespace FlashSentry {

static QString s_configFilePath;
static bool s_jsonOutput = false;
static bool s_quietOutput = false;

void VerifyCli::setConfigFilePath(const QString& path)
{
    s_configFilePath = path;
}

void VerifyCli::setJsonOutput(bool enabled)
{
    s_jsonOutput = enabled;
}

void VerifyCli::setQuietOutput(bool enabled)
{
    s_quietOutput = enabled;
}

bool VerifyCli::jsonOutput()
{
    return s_jsonOutput;
}

bool VerifyCli::quietOutput()
{
    return s_quietOutput;
}

void VerifyCli::applyUserSettings()
{
    IsoVerifySettingsLoader::applyToVerifier(s_configFilePath);
}

static int resultsToExitCode(const QList<IsoVerifyResult>& results)
{
    if (results.isEmpty()) {
        if (!VerifyCli::jsonOutput()) {
            std::cerr << "No images found to verify.\n";
        }
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
    if (VerifyCli::jsonOutput()) {
        std::cout << IsoVerifyReport::buildJson(results).toStdString() << '\n';
        return;
    }
    if (VerifyCli::quietOutput()) {
        std::cout << IsoVerifyReport::summaryLine(results).toStdString() << '\n';
        return;
    }
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
    if (jsonOutput()) {
        QJsonObject obj;
        obj.insert(QStringLiteral("ok"), ok);
        obj.insert(QStringLiteral("entry_count"), IsoCatalogManifest::entryCount());
        obj.insert(QStringLiteral("embedded_integrity_ok"), IsoCatalogManifest::lastEmbeddedIntegrityOk());
        std::cout << QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString() << '\n';
    } else if (ok) {
        std::cout << "ISO catalog updated (" << IsoCatalogManifest::entryCount() << " entries).\n";
    } else {
        std::cerr << "Failed to refresh ISO catalog manifest.\n";
    }
    return ok ? VerifyCli::ExitOk : VerifyCli::ExitError;
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
    if (jsonOutput() || fmt == QStringLiteral("json")) {
        content = IsoVerifyReport::buildJson(results);
    } else if (fmt == QStringLiteral("html")) {
        content = IsoVerifyReport::buildHtml(results);
    } else if (fmt == QStringLiteral("csv")) {
        content = IsoVerifyReport::buildCsv(results);
    } else if (quietOutput()) {
        content = IsoVerifyReport::summaryLine(results);
    } else {
        content = IsoVerifyReport::buildPlainText(results);
    }

    std::cout << content.toStdString();
    if (!content.isEmpty() && !content.endsWith(QLatin1Char('\n'))) {
        std::cout << '\n';
    }
    return resultsToExitCode(results);
}

int VerifyCli::runListPublishers()
{
    IsoCatalogManifest::ensureLoaded();
    const QStringList ids = IsoCatalog::knownPublisherIds();
    const bool integrityOk = IsoCatalogManifest::lastEmbeddedIntegrityOk();

    if (jsonOutput()) {
        QJsonObject root;
        QJsonArray pub;
        for (const QString& id : ids) {
            pub.append(id);
        }
        root.insert(QStringLiteral("publishers"), pub);
        root.insert(QStringLiteral("manifest_entries"), IsoCatalogManifest::entryCount());
        root.insert(QStringLiteral("embedded_integrity_ok"), integrityOk);
        std::cout << QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString() << '\n';
    } else {
        std::cout << "Built-in publisher IDs (" << ids.size() << "):\n";
        for (const QString& id : ids) {
            std::cout << "  " << id.toStdString() << '\n';
        }
        std::cout << "Manifest entries: " << IsoCatalogManifest::entryCount() << '\n';
    }

    if (!integrityOk) {
        if (!jsonOutput()) {
            std::cerr << "Warning: embedded manifest integrity check failed.\n";
        }
        return ExitError;
    }
    return ExitOk;
}

int VerifyCli::runTrustHash(const QString& fileName, const QString& sha256Hex)
{
    IsoCatalogManifest::ensureLoaded();
    const QString base = QFileInfo(fileName).fileName();
    if (base.isEmpty() || sha256Hex.size() != 64) {
        if (!jsonOutput()) {
            std::cerr << "Usage: trust-hash requires a filename and 64-character SHA-256 hex.\n";
        }
        return ExitError;
    }
    const bool ok = IsoCatalogManifest::trustUserHash(base, sha256Hex);
    if (jsonOutput()) {
        QJsonObject obj;
        obj.insert(QStringLiteral("ok"), ok);
        obj.insert(QStringLiteral("file"), base);
        std::cout << QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString() << '\n';
    } else if (ok) {
        std::cout << "Trusted hash saved for " << base.toStdString() << '\n';
    } else if (!quietOutput()) {
        std::cerr << "Failed to save trusted hash.\n";
    }
    return ok ? ExitOk : ExitError;
}

} // namespace FlashSentry
