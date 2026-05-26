#include "SettingsProfiles.h"

namespace FlashSentry {

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
        settings.autoHashOnConnect = false;
        settings.requireConfirmationForModified = true;
        return;
    }
    if (id == QStringLiteral("paranoid")) {
        settings.appModule = AppModule::UsbMonitor;
        settings.defaultVerificationProfile = VerificationProfile::WatchManifest;
        settings.isoAutoVerifyOnUsbMount = true;
        settings.blockMountOnIsoVerifyFailure = true;
        settings.blockModifiedDevices = true;
        settings.requireConfirmationForModified = true;
        settings.isoPreferOfflineSidecars = false;
        settings.isoVerifyParallel = 1;
        return;
    }
}

} // namespace FlashSentry
