#pragma once

#include "Types.h"

namespace FlashSentry {

class SettingsProfiles {
public:
    static void applyProfile(const QString& profileId, AppSettings& settings);
    static QStringList profileIds();
    static QString profileDisplayName(const QString& profileId);
};

} // namespace FlashSentry
