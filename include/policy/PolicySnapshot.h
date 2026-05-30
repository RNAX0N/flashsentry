#pragma once

#include "BlockedDriveStore.h"
#include "Types.h"

#include <QList>

namespace FlashSentry::Policy {

/** In-memory authoritative policy state. */
struct PolicySnapshot {
    QList<DeviceRecord> devices;
    QList<BlockedDriveEntry> blocks;
};

} // namespace FlashSentry::Policy
