#pragma once

#include <QString>

namespace FlashSpartan {

/** Headless verification and catalog maintenance (exit codes for scripts). */
class VerifyCli {
public:
    static constexpr int ExitOk = 0;
    static constexpr int ExitVerifyFailed = 1;
    static constexpr int ExitError = 2;

    static int runVerifyIso(const QString& path);
    static int runVerifyMount(const QString& mountPoint);
    static int runVerifyDir(const QString& directory);
    static int runUpdateCatalog(bool force);
    static int runExportReport(const QString& path, const QString& format);
    static int runListPublishers();
    static int runTrustHash(const QString& fileName, const QString& sha256Hex);

    /** Optional FlashSpartan.conf path for subsequent CLI verify commands. */
    static void setConfigFilePath(const QString& path);

    /** Machine-readable stdout for verify/export commands. */
    static void setJsonOutput(bool enabled);

    /** Print only summary lines (no per-file report body). */
    static void setQuietOutput(bool enabled);

    static bool jsonOutput();
    static bool quietOutput();

    /** Apply ISO verify keys from QSettings (uses setConfigFilePath when set). */
    static void applyUserSettings();
};

} // namespace FlashSpartan
