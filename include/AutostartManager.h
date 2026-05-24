#pragma once

#include <QString>
#include <optional>

namespace FlashSentry {

/**
 * @brief Enables or disables FlashSentry at graphical login.
 *
 * Prefers the packaged systemd user unit (flashsentry.service). Falls back to
 * an XDG autostart desktop entry when systemd is unavailable (e.g. dev builds).
 */
class AutostartManager {
public:
    enum class Backend { None, Systemd, Xdg };

    static bool isAvailable();
    static Backend backend();

    static std::optional<bool> isLoginAutostartEnabled();
    static bool setLoginAutostartEnabled(bool enabled, QString* errorMessage = nullptr);

    static QString backendDescription();
};

} // namespace FlashSentry
