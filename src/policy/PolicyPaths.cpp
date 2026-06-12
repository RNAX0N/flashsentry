#include "policy/PolicyPaths.h"

#include "AppPaths.h"

#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace FlashSpartan::Policy {

QString PolicyPaths::configDir()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains(QStringLiteral("FLASHSPARTAN_POLICY_CONFIG"))) {
        return env.value(QStringLiteral("FLASHSPARTAN_POLICY_CONFIG"));
    }
    return AppPaths::configDir();
}

QString PolicyPaths::storeFilePath()
{
    return configDir() + QStringLiteral("/policy.store");
}

QString PolicyPaths::keyFilePath()
{
    return configDir() + QStringLiteral("/policy.key");
}

QString PolicyPaths::auditLogPath()
{
    return configDir() + QStringLiteral("/policy-audit.log");
}

QString PolicyPaths::legacyDevicesJsonPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/FlashSentry/flashsentry/devices.json");
}

QString PolicyPaths::legacyBlockedJsonPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/FlashSentry/blocked-drives.json");
}

QString PolicyPaths::socketPath()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    return runtime + QStringLiteral("/flashspartan-policy.sock");
}

QString PolicyPaths::tokenPath()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    return runtime + QStringLiteral("/flashspartan-policy.token");
}

} // namespace FlashSpartan::Policy
