#include "CrashReporter.h"

#include <QByteArray>
#include <QProcessEnvironment>
#include <QDebug>

namespace FlashSpartan::CrashReporter {

void tryInstall(const AppSettings& settings)
{
    if (!settings.crashReportsEnabled) {
        return;
    }

    const QByteArray dsn = qgetenv("FLASHSPARTAN_SENTRY_DSN");
    if (dsn.isEmpty()) {
        qInfo() << "Crash reports enabled but FLASHSPARTAN_SENTRY_DSN is not set;"
                << "see docs/DIAGNOSTICS.md";
        return;
    }

#ifdef FLASHSPARTAN_SENTRY
    Q_UNUSED(dsn)
    qInfo() << "Sentry crash reporting would initialize here (FLASHSPARTAN_SENTRY build)";
#else
    qInfo() << "Crash reports requested; rebuild with -DFLASHSPARTAN_SENTRY=ON and link sentry-native";
    Q_UNUSED(dsn)
#endif
}

} // namespace FlashSpartan::CrashReporter
