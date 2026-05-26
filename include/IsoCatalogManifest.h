#pragma once

#include "IsoCatalog.h"

#include <optional>

namespace FlashSentry {

/**
 * @brief Embedded and remotely updatable SHA-256 catalog (Windows and extensions).
 */
class IsoCatalogManifest {
public:
    static std::optional<IsoPublisherMatch> lookup(const QString& fileName);

    static void ensureLoaded();
    static void reload();

    /** Fetch remote_url into cache. When force=true, ignore cache age. */
    static bool refreshRemoteIfStale(int maxAgeSeconds = 7 * 24 * 3600, bool force = false);

    static int entryCount();

    /** Save user-trusted hash for exact filename (TOFU). */
    static bool trustUserHash(const QString& fileName, const QString& sha256Hex);

    static bool lastEmbeddedIntegrityOk();
};

} // namespace FlashSentry
