#pragma once

#include "MountTypes.h"

#include <QVariantMap>

namespace FlashSpartan {

/** UDisks2 mount/unmount option maps (testable without D-Bus). */
class MountOptionsUtil {
public:
    static QVariantMap toUdisksMountOptions(const MountOptions& options);
    static QVariantMap toUdisksUnmountOptions(const UnmountOptions& options);
};

} // namespace FlashSpartan
