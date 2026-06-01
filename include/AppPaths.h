#pragma once

#include <QString>

namespace FlashSpartan {

/** User config directory and one-time migration from FlashSentry. */
class AppPaths {
public:
    /** ~/.config/FlashSpartan (or %AppData%/FlashSpartan on Windows). */
    static QString configDir();
    /** Pre-1.5.0 install location; used only for migration reads. */
    static QString legacyConfigDir();

    /** Copy known data files from ~/.config/FlashSentry when new paths are missing. */
    static void migrateFromLegacyConfigIfNeeded();

    static void migrateQSettingsFromFlashSentry();
};

} // namespace FlashSpartan
