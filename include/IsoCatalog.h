#pragma once

#include <QString>
#include <QStringList>
#include <QRegularExpression>

#include <optional>
namespace FlashSentry {

struct IsoPublisherMatch {
    QString publisherId;
    QString publisherName;
    QString releaseLabel;
    QString isoFileName;
    QString checksumUrl;
    QString signatureUrl;
    QStringList signingKeyIds;
    QStringList trustedFingerprints;
    /** When true, checksumUrl/signatureUrl name the ISO's .sha256 and .sig (not a SUMS file). */
    bool perFileArtifacts = false;
    /** When set, expected hash comes from the embedded/remote manifest (no checksum download). */
    QString embeddedSha256;
    /** Filename matched but hash must come from a sidecar or catalog update (e.g. Windows). */
    bool hintOnly = false;
    QString referenceUrl;
};

/**
 * @brief Maps ISO filenames to official checksum/signature URLs and trusted keys.
 */
class IsoCatalog {
public:
    static std::optional<IsoPublisherMatch> matchIso(const QString& isoPath);

    static QStringList knownPublisherIds();
};

} // namespace FlashSentry
