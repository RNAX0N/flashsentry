#pragma once

#include <QString>

namespace FlashSentry {

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
};

} // namespace FlashSentry
