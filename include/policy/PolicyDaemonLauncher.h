#pragma once

#include <QString>

namespace FlashSpartan::Policy {

/** Ensures flashspartan-policyd is running and socket is reachable. */
class PolicyDaemonLauncher {
public:
    static bool ensureRunning(QString* error = nullptr);
    static QString daemonExecutablePath();
};

} // namespace FlashSpartan::Policy
