#pragma once

#include <QString>
#include <optional>

namespace FlashSpartan {

/**
 * @brief Enables or disables FlashSpartan at graphical login.
 *
 * Linux: prefers systemd user unit (flashspartan.service), falls back to XDG autostart.
 * Windows: current-user Run registry key.
 */
class AutostartManager {
public:
    enum class Backend { None, Systemd, Xdg, WindowsRegistry };

    static bool isAvailable();
    static Backend backend();

    static std::optional<bool> isLoginAutostartEnabled();
    static bool setLoginAutostartEnabled(bool enabled, QString* errorMessage = nullptr);

    static QString backendDescription();
};

} // namespace FlashSpartan
