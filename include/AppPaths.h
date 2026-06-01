#pragma once

#include <QString>

namespace FlashSpartan {

/** User config directory and one-time migration from FlashSentry. */
class AppPaths {
public:
    static QString configDir();
    static QString legacyConfigDir();

    /** Copy known data files from ~/.config/FlashSentry when new paths are missing. */
    static void migrateFromLegacyConfigIfNeeded();

    static void migrateQSettingsFromFlashSentry();
};

} // namespace FlashSpartan
