#pragma once

#include "Types.h"

namespace FlashSentry {

class SettingsProfiles {
public:
    static void applyProfile(const QString& profileId, AppSettings& settings);
    static QStringList profileIds();
    static QString profileDisplayName(const QString& profileId);
    static QString profileDescription(const QString& profileId);

    /** Maps legacy ids (e.g. `ventoy`) to current profile ids. */
    static QString normalizeProfileId(const QString& profileId);
};

} // namespace FlashSentry
