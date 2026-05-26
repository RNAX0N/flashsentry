#pragma once

#include "IsoVerifyOptions.h"

#include <QString>

namespace FlashSentry {

/** Load ISO verify options from FlashSentry.conf (same keys as the GUI). */
class IsoVerifySettingsLoader {
public:
    static IsoVerifyOptions load(const QString& configFilePath = {});
    static void applyToVerifier(const QString& configFilePath = {});
};

} // namespace FlashSentry
