#include "policy/PolicyPaths.h"

#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace FlashSentry::Policy {

QString PolicyPaths::configDir()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains(QStringLiteral("FLASHSENTRY_POLICY_CONFIG"))) {
        return env.value(QStringLiteral("FLASHSENTRY_POLICY_CONFIG"));
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/FlashSentry");
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
    return configDir() + QStringLiteral("/flashsentry/devices.json");
}

QString PolicyPaths::legacyBlockedJsonPath()
{
    return configDir() + QStringLiteral("/blocked-drives.json");
}

QString PolicyPaths::socketPath()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    return runtime + QStringLiteral("/flashsentry-policy.sock");
}

} // namespace FlashSentry::Policy
