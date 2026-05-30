#pragma once

#include "policy/PolicyGateway.h"

#include <memory>

namespace FlashSentry::Policy {

/** Process-wide installed policy gate (daemon client or in-process for tests). */
class PolicyServiceLocator {
public:
    static void install(std::unique_ptr<PolicyGateway> gateway);
    static PolicyGateway* gateway();
    static bool hasGateway();

private:
    PolicyServiceLocator() = delete;
};

} // namespace FlashSentry::Policy
