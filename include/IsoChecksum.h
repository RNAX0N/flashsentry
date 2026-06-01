#pragma once

#include <QString>

namespace FlashSpartan {

/**
 * @brief Parses publisher and sidecar SHA-256 checksum list content.
 */
class IsoChecksum {
public:
    /** Find hash for isoBaseName in SUMS-style content, or a single 64-hex line. */
    static QString parseSha256Content(const QString& content, const QString& isoBaseName,
                                      QString* errorOut = nullptr);
};

} // namespace FlashSpartan
