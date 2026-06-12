#include "IsoVerifyUi.h"

#include "IsoVerifyReport.h"

namespace FlashSpartan {

IsoVerifyUi::Outcome IsoVerifyUi::outcome(const IsoVerifyResult& result)
{
    if (result.inconclusive()) {
        return Outcome::NotVerified;
    }
    if (result.passed()) {
        return Outcome::Verified;
    }
    return Outcome::Failed;
}

QString IsoVerifyUi::outcomeLabel(const IsoVerifyResult& result)
{
    switch (outcome(result)) {
    case Outcome::Verified:
        return QStringLiteral("Verified");
    case Outcome::Failed:
        return QStringLiteral("Failed");
    case Outcome::NotVerified:
        return QStringLiteral("Not verified");
    }
    return QStringLiteral("Unknown");
}

QString IsoVerifyUi::outcomeExplanation(const IsoVerifyResult& result)
{
    if (!result.success && !result.errorMessage.isEmpty()) {
        return result.errorMessage;
    }
    if (result.baselineChecked) {
        if (!result.baselineMatches) {
            return QStringLiteral(
                "This file no longer matches the hash recorded the last time this USB stick was verified.");
        }
        return QStringLiteral("This file matches the hash recorded on this USB stick during a prior visit.");
    }

    switch (outcome(result)) {
    case Outcome::Verified:
        if (result.pgpChecked) {
            return QStringLiteral(
                "The file matches the publisher checksum and the signature is from a trusted signing key.");
        }
        return QStringLiteral("The file matches the publisher checksum.");
    case Outcome::Failed:
        if (result.hashChecked && !result.expectedSha256.isEmpty() && !result.hashMatches) {
            return QStringLiteral(
                "The file does not match the publisher checksum — it may be corrupt, incomplete, or the wrong image.");
        }
        if (result.pgpChecked && !result.pgpValid) {
            return QStringLiteral("The checksum signature is missing or invalid.");
        }
        if (result.pgpChecked && !result.fingerprintTrusted) {
            return QStringLiteral("The signing key is not on FlashSpartan's trusted list for this publisher.");
        }
        if (!result.errorMessage.isEmpty()) {
            return result.errorMessage;
        }
        return QStringLiteral("Verification did not pass.");
    case Outcome::NotVerified:
        return QStringLiteral(
            "FlashSpartan computed a SHA-256 hash but had no publisher checksum to compare against.");
    }
    return {};
}

QString IsoVerifyUi::nextStepHint(const IsoVerifyResult& result)
{
    switch (outcome(result)) {
    case Outcome::Verified:
        return {};
    case Outcome::Failed:
        if (result.baselineChecked && !result.baselineMatches) {
            return QStringLiteral(
                "If you replaced the image intentionally, verify again to update the stick baseline.");
        }
        if (result.hashChecked && !result.hashMatches) {
            return QStringLiteral("Re-download the image from the publisher and copy it again.");
        }
        if (result.pgpChecked && !result.fingerprintTrusted) {
            return QStringLiteral(
                "Confirm the signing key on the publisher website, or use More → Update catalog.");
        }
        if (result.pgpChecked && !result.pgpValid) {
            return QStringLiteral(
                "Place a valid signature file next to the image (for example SHA256SUMS.gpg) and verify again.");
        }
        return QStringLiteral("Check the full report for details, then try verifying again.");
    case Outcome::NotVerified:
        return QStringLiteral(
            "Add a .sha256 file next to the image, use More → Trust hash, or pick a filename from a supported "
            "publisher list.");
    }
    return {};
}

QString IsoVerifyUi::hashColumnText(const IsoVerifyResult& result)
{
    if (!result.hashChecked) {
        return QStringLiteral("—");
    }
    if (result.expectedSha256.isEmpty()) {
        return QStringLiteral("No reference");
    }
    return result.hashMatches ? QStringLiteral("Matches") : QStringLiteral("Mismatch");
}

QString IsoVerifyUi::pgpColumnText(const IsoVerifyResult& result)
{
    if (!result.pgpChecked) {
        return QStringLiteral("—");
    }
    return result.pgpValid ? QStringLiteral("Valid") : QStringLiteral("Invalid");
}

QString IsoVerifyUi::keyColumnText(const IsoVerifyResult& result)
{
    if (!result.pgpChecked) {
        return QStringLiteral("—");
    }
    return result.fingerprintTrusted ? QStringLiteral("Trusted") : QStringLiteral("Unknown");
}

QString IsoVerifyUi::summaryLine(int passed, int total, int notVerified)
{
    if (total <= 0) {
        return QStringLiteral("No images found to verify.");
    }

    QString line;
    if (passed == total) {
        line = total == 1 ? QStringLiteral("1 image verified")
                          : QStringLiteral("All %1 images verified").arg(total);
    } else {
        line = QStringLiteral("%1 of %2 images verified").arg(passed).arg(total);
    }

    const int failed = total - passed - notVerified;
    QStringList parts;
    if (failed > 0) {
        parts << (failed == 1 ? QStringLiteral("1 failed")
                              : QStringLiteral("%1 failed").arg(failed));
    }
    if (notVerified > 0) {
        parts << (notVerified == 1
                      ? QStringLiteral("1 could not be verified (no publisher checksum)")
                      : QStringLiteral("%1 could not be verified (no publisher checksum)").arg(notVerified));
    }
    if (!parts.isEmpty()) {
        line += QStringLiteral(" · ") + parts.join(QStringLiteral(" · "));
    }
    return line;
}

QString IsoVerifyUi::summaryLine(const QList<IsoVerifyResult>& results)
{
    const IsoVerifyReport::SummaryCounts counts = IsoVerifyReport::countSummary(results);
    return summaryLine(counts.passed, counts.total, counts.needsSidecar);
}

QString IsoVerifyUi::verifiedChipText(int count)
{
    return count == 1 ? QStringLiteral("✓ 1 verified") : QStringLiteral("✓ %1 verified").arg(count);
}

QString IsoVerifyUi::failedChipText(int count)
{
    return count == 1 ? QStringLiteral("✗ 1 failed") : QStringLiteral("✗ %1 failed").arg(count);
}

QString IsoVerifyUi::notVerifiedChipText(int count)
{
    return count == 1 ? QStringLiteral("? 1 not verified")
                      : QStringLiteral("? %1 not verified").arg(count);
}

QString IsoVerifyUi::introHtml()
{
    return QStringLiteral(
        "Check install images (<b>.iso</b>, <b>.img.xz</b>, and similar) on any USB stick — whether you used "
        "<b>dd</b>, Rufus, a file copy, or another tool. FlashSpartan compares each file to publisher checksums "
        "and signatures when available.");
}

QString IsoVerifyUi::legendHtml()
{
    return QStringLiteral(
        "<b>How to read results</b><br>"
        "<span style='color:#2e7d32'>● Verified</span> — matches the publisher checksum"
        " (and signature when checked).<br>"
        "<span style='color:#c62828'>● Failed</span> — checksum mismatch, bad signature, or untrusted signing "
        "key.<br>"
        "<span style='color:#e65100'>● Not verified</span> — hash was computed but no publisher checksum was "
        "available; add a <code>.sha256</code> sidecar or use <b>More → Trust hash</b>.");
}

QString IsoVerifyUi::tableHeaderTooltip(int column)
{
    switch (column) {
    case 0:
        return QStringLiteral("Image filename on the USB stick or chosen folder.");
    case 1:
        return QStringLiteral("Matched Linux distro or publisher from the built-in catalog.");
    case 2:
        return QStringLiteral(
            "Whether the file's SHA-256 matches the publisher checksum. “No reference” means nothing to compare "
            "against.");
    case 3:
        return QStringLiteral("Whether the publisher's checksum file has a valid OpenPGP signature.");
    case 4:
        return QStringLiteral("Whether the signing key fingerprint is on FlashSpartan's trusted list.");
    case 5:
        return QStringLiteral("Overall result for this image. Click a row to see the detailed report below.");
    default:
        return {};
    }
}

QString IsoVerifyUi::trayTitle()
{
    return QStringLiteral("Image verification");
}

QString IsoVerifyUi::trayMessage(const QString& deviceName, int passed, int total, int notVerified)
{
    const QString summary = summaryLine(passed, total, notVerified);
    if (deviceName.isEmpty()) {
        return summary;
    }
    return QStringLiteral("%1 — %2").arg(deviceName, summary);
}

} // namespace FlashSpartan
