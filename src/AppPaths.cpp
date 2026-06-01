#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace FlashSpartan {

QString AppPaths::configDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString dir = base + QStringLiteral("/FlashSpartan");
    QDir().mkpath(dir);
    return dir;
}

QString AppPaths::legacyConfigDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/FlashSentry");
}

void AppPaths::migrateFromLegacyConfigIfNeeded()
{
    const QString legacy = legacyConfigDir();
    const QString current = configDir();
    if (!QDir(legacy).exists()) {
        return;
    }

    const auto copyIfMissing = [&](const QString& fileName) {
        const QString dest = current + QLatin1Char('/') + fileName;
        if (QFile::exists(dest)) {
            return;
        }
        const QString src = legacy + QLatin1Char('/') + fileName;
        if (QFile::exists(src)) {
            QFile::copy(src, dest);
        }
    };

    copyIfMissing(QStringLiteral("device-timeline.json"));
    copyIfMissing(QStringLiteral("verify-history.json"));
    copyIfMissing(QStringLiteral("hash-checkpoints.json"));
    copyIfMissing(QStringLiteral("blocked-drives.json"));

    const QString legacyDevices = legacy + QStringLiteral("/flashsentry/devices.json");
    const QString newDevicesDir = current + QStringLiteral("/flashspartan");
    const QString newDevices = newDevicesDir + QStringLiteral("/devices.json");
    if (QFile::exists(legacyDevices) && !QFile::exists(newDevices)) {
        QDir().mkpath(newDevicesDir);
        QFile::copy(legacyDevices, newDevices);
    }
}

void AppPaths::migrateQSettingsFromFlashSentry()
{
    QSettings oldSettings(QStringLiteral("flashsentry"), QStringLiteral("FlashSentry"));
    QSettings newSettings(QStringLiteral("flashspartan"), QStringLiteral("FlashSpartan"));
    if (!newSettings.allKeys().isEmpty() || oldSettings.allKeys().isEmpty()) {
        return;
    }
    for (const QString& key : oldSettings.allKeys()) {
        newSettings.setValue(key, oldSettings.value(key));
    }
    newSettings.sync();
}

} // namespace FlashSpartan
