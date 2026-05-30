#pragma once

#include <QString>

namespace FlashSentry::Policy {

/** Ensures flashsentry-policyd is running and socket is reachable. */
class PolicyDaemonLauncher {
public:
    static bool ensureRunning(QString* error = nullptr);
    static QString daemonExecutablePath();
};

} // namespace FlashSentry::Policy
