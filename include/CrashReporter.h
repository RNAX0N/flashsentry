#pragma once

#include "Types.h"

namespace FlashSpartan::CrashReporter {

/** Opt-in crash reporting hook (Sentry when built with FLASHSPARTAN_SENTRY). */
void tryInstall(const AppSettings& settings);

} // namespace FlashSpartan::CrashReporter
