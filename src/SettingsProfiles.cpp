#include "SettingsProfiles.h"

namespace FlashSentry {

QStringList SettingsProfiles::profileIds()
{
    return {QStringLiteral("default"),
            QStringLiteral("ventoy"),
            QStringLiteral("work_usb"),
            QStringLiteral("paranoid")};
}

QString SettingsProfiles::profileDisplayName(const QString& profileId)
{
    if (profileId == QStringLiteral("ventoy")) {
        return QStringLiteral("Multi-image USB (several ISOs on one stick)");
    }
    if (profileId == QStringLiteral("work_usb")) {
        return QStringLiteral("Work USB (watch folders)");
    }
    if (profileId == QStringLiteral("paranoid")) {
        return QStringLiteral("Paranoid");
    }
    return QStringLiteral("Default (recommended)");
}

void SettingsProfiles::applyProfile(const QString& profileId, AppSettings& settings)
{
    settings.settingsProfile = profileId;
    if (profileId == QStringLiteral("ventoy")) {
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
    if (profileId == QStringLiteral("work_usb")) {
        settings.appModule = AppModule::UsbMonitor;
        settings.defaultVerificationProfile = VerificationProfile::WatchManifest;
        settings.isoAutoVerifyOnUsbMount = false;
        settings.autoHashOnConnect = false;
        settings.requireConfirmationForModified = true;
        return;
    }
    if (profileId == QStringLiteral("paranoid")) {
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
