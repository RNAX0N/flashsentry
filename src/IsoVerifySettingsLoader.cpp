#include "IsoVerifySettingsLoader.h"

#include "IsoVerifier.h"

#include <QSettings>

namespace FlashSpartan {

IsoVerifyOptions IsoVerifySettingsLoader::load(const QString& configFilePath)
{
    IsoVerifyOptions opt;
    opt.useHashCache = true;

    if (!configFilePath.isEmpty()) {
        QSettings settings(configFilePath, QSettings::IniFormat);
        opt.maxParallel = qMax(1, settings.value(QStringLiteral("iso/verifyParallel"), 2).toInt());
        opt.verifyDecompressed = settings.value(QStringLiteral("iso/verifyDecompressed"), false).toBool();
        opt.preferOfflineSidecars =
            settings.value(QStringLiteral("iso/preferOfflineSidecars"), false).toBool();
        return opt;
    }

    QSettings settings(QStringLiteral("flashspartan"), QStringLiteral("FlashSpartan"));
    opt.maxParallel = qMax(1, settings.value(QStringLiteral("iso/verifyParallel"), 2).toInt());
    opt.verifyDecompressed = settings.value(QStringLiteral("iso/verifyDecompressed"), false).toBool();
    opt.preferOfflineSidecars =
        settings.value(QStringLiteral("iso/preferOfflineSidecars"), false).toBool();
    return opt;
}

void IsoVerifySettingsLoader::applyToVerifier(const QString& configFilePath)
{
    IsoVerifier::setVerifyOptions(load(configFilePath));
}

} // namespace FlashSpartan
