#pragma once

#include <QString>

namespace FlashSentry::Policy {

/** Paths for the gated policy store (not exposed to UI code). */
class PolicyPaths {
public:
    static QString configDir();
    static QString storeFilePath();
    static QString keyFilePath();
    static QString auditLogPath();
    static QString legacyDevicesJsonPath();
    static QString legacyBlockedJsonPath();
    static QString socketPath();
};

} // namespace FlashSentry::Policy
