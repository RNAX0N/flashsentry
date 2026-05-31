#pragma once

#include <QtGlobal>

namespace FlashSentryTest {

/** GPG from MSVC/Qt is flaky on GitHub Windows runners; covered by validate-iso-manifest.py. */
inline bool skipGpgAssertionsOnWindowsCi()
{
#if defined(Q_OS_WIN)
    return qEnvironmentVariableIsSet("GITHUB_ACTIONS");
#else
    return false;
#endif
}

} // namespace FlashSentryTest
