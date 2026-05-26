#pragma once

#include "IsoCatalog.h"

#include <optional>

namespace FlashSentry {

/**
 * @brief Embedded and remotely updatable SHA-256 catalog (Windows and other static hashes).
 *
 * Microsoft does not publish stable per-file checksum URLs; this manifest ships known hashes
 * and can merge entries from remote_url (see resources/iso-catalog/embedded-manifest.json).
 */
class IsoCatalogManifest {
public:
    static std::optional<IsoPublisherMatch> lookup(const QString& fileName);

    /** Reload from disk cache if present; otherwise embedded Qt resource. */
    static void ensureLoaded();

    /** Fetch remote_url into cache when older than maxAgeSeconds (default 7 days). */
    static bool refreshRemoteIfStale(int maxAgeSeconds = 7 * 24 * 3600);

    static int entryCount();
};

} // namespace FlashSentry
