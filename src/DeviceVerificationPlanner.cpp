#include "DeviceVerificationPlanner.h"

namespace FlashSpartan {

DeviceVerificationPlanner::Plan DeviceVerificationPlanner::planForDevice(VerificationProfile profile,
                                                                       bool mountPointEmpty,
                                                                       bool isMounted)
{
    Plan plan;
    plan.profile = profile;

    if (profile == VerificationProfile::FullPartition) {
        plan.action = StartAction::StartFullPartitionHash;
        return plan;
    }

    if (mountPointEmpty) {
        plan.action = StartAction::MountThenVerify;
        return plan;
    }

    Q_UNUSED(isMounted);
    plan.action = StartAction::StartManifestVerification;
    return plan;
}

} // namespace FlashSpartan
