#include "policy/PolicyServiceLocator.h"

namespace FlashSpartan::Policy {

namespace {

std::unique_ptr<PolicyGateway> g_gateway;

}

void PolicyServiceLocator::install(std::unique_ptr<PolicyGateway> gateway)
{
    g_gateway = std::move(gateway);
}

PolicyGateway* PolicyServiceLocator::gateway()
{
    return g_gateway.get();
}

bool PolicyServiceLocator::hasGateway()
{
    return static_cast<bool>(g_gateway);
}

} // namespace FlashSpartan::Policy
