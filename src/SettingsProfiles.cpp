#include "SettingsProfiles.h"

namespace FlashSpartan {

QString SettingsProfiles::normalizeProfileId(const QString& profileId)
{
    if (profileId == QStringLiteral("ventoy")) {
        return QStringLiteral("multi_image");
    }
    return profileId;
}

QStringList SettingsProfiles::profileIds()
{
    return {QStringLiteral("default"),
            QStringLiteral("multi_image"),
            QStringLiteral("work_usb"),
            QStringLiteral("paranoid")};
}

QString SettingsProfiles::profileDisplayName(const QString& profileId)
{
    const QString id = normalizeProfileId(profileId);
    if (id == QStringLiteral("multi_image")) {
        return QStringLiteral("Multi-image USB");
    }
    if (id == QStringLiteral("work_usb")) {
        return QStringLiteral("Work USB (watch folders)");
    }
    if (id == QStringLiteral("paranoid")) {
        return QStringLiteral("Paranoid");
    }
    return QStringLiteral("Default (recommended)");
}

QString SettingsProfiles::profileDescription(const QString& profileId)
{
    const QString id = normalizeProfileId(profileId);
    if (id == QStringLiteral("multi_image")) {
        return QStringLiteral(
            "Auto-verify images on USB mount (dd, Rufus, copied ISOs, multiboot sticks). "
            "Skips full-disk hashing on connect. Prefers offline checksum sidecars when present.");
    }
    if (id == QStringLiteral("work_usb")) {
        return QStringLiteral(
            "Focus on Merkle watch-folder baselines. ISO auto-verify on mount is off. "
            "Good for project drives with a known directory layout.");
    }
    if (id == QStringLiteral("paranoid")) {
        return QStringLiteral(
            "Maximum caution: hash on connect, ISO verify on mount, block mounting when "
            "fingerprints or image checks fail, confirm tampering, single parallel verify job.");
    }
    return QStringLiteral(
        "Balanced defaults: ISO verify on USB mount, watch-folder verification, no automatic "
        "full-partition hash on every plug-in.");
}

void SettingsProfiles::applyProfile(const QString& profileId, AppSettings& settings)
{
    const QString id = normalizeProfileId(profileId);
    settings.settingsProfile = id;
    if (id == QStringLiteral("multi_image")) {
        settings.appModule = AppModule::UsbMonitor;
        settings.defaultVerificationProfile = VerificationProfile::WatchManifest;
        settings.isoAutoVerifyOnUsbMount = true;
        settings.isoAutoVerifyOnScan = true;
        settings.autoHashOnConnect = false;
        settings.isoVerifyParallel = 2;
        settings.blockMountOnIsoVerifyFailure = false;
        settings.isoPreferOfflineSidecars = true;
        settings.blockModifiedDevices = false;
        return;
    }
    if (id == QStringLiteral("work_usb")) {
        settings.appModule = AppModule::UsbMonitor;
        settings.defaultVerificationProfile = VerificationProfile::WatchManifest;
        settings.isoAutoVerifyOnUsbMount = false;
        settings.isoAutoVerifyOnScan = false;
        settings.autoHashOnConnect = false;
        settings.requireConfirmationForModified = true;
        return;
    }
    if (id == QStringLiteral("paranoid")) {
        settings.appModule = AppModule::UsbMonitor;
        settings.defaultVerificationProfile = VerificationProfile::WatchManifest;
        settings.autoHashOnConnect = true;
        settings.autoHashOnEject = true;
        settings.isoAutoVerifyOnUsbMount = true;
        settings.isoAutoVerifyOnScan = true;
        settings.blockMountOnIsoVerifyFailure = true;
        settings.blockModifiedDevices = true;
        settings.requireConfirmationForNew = true;
        settings.requireConfirmationForModified = true;
        settings.isoPreferOfflineSidecars = false;
        settings.isoVerifyParallel = 1;
        settings.maxConcurrentHashes = 1;
        return;
    }
}

} // namespace FlashSpartan
