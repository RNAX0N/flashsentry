#pragma once

#include "Types.h"

namespace FlashSpartan {

/** Decides how USB device verification should start (no I/O). */
class DeviceVerificationPlanner {
public:
    enum class StartAction {
        StartFullPartitionHash,
        MountThenVerify,
        StartManifestVerification,
    };

    struct Plan {
        StartAction action = StartAction::StartManifestVerification;
        VerificationProfile profile = VerificationProfile::WatchManifest;
    };

    static Plan planForDevice(VerificationProfile profile, bool mountPointEmpty, bool isMounted);
};

} // namespace FlashSpartan
