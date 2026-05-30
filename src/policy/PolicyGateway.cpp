#include "policy/PolicyGateway.h"
#include "policy/PolicyDaemonClient.h"
#include "policy/PolicyDaemonLauncher.h"
#include "policy/PolicyInProcessGateway.h"

#include <QProcessEnvironment>

namespace FlashSentry::Policy {

std::unique_ptr<PolicyGateway> PolicyGateway::createInProcess(const QString& storePath)
{
    return std::make_unique<PolicyInProcessGateway>(storePath);
}

std::unique_ptr<PolicyGateway> PolicyGateway::createDefault()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.value(QStringLiteral("FLASHSENTRY_POLICY_IN_PROCESS")) == QStringLiteral("1")) {
        return createInProcess(env.value(QStringLiteral("FLASHSENTRY_POLICY_STORE")));
    }

    QString err;
    if (!PolicyDaemonLauncher::ensureRunning(&err)) {
        // Fallback for dev without installed daemon
        return createInProcess();
    }
    return std::make_unique<PolicyDaemonClient>();
}

} // namespace FlashSentry::Policy
