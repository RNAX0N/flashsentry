#pragma once

#include "IsoVerifyOptions.h"

#include <QString>

namespace FlashSpartan {

/** Load ISO verify options from FlashSpartan.conf (same keys as the GUI). */
class IsoVerifySettingsLoader {
public:
    static IsoVerifyOptions load(const QString& configFilePath = {});
    static void applyToVerifier(const QString& configFilePath = {});
};

} // namespace FlashSpartan
