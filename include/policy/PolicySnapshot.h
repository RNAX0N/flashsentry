#pragma once

#include "BlockedDriveStore.h"
#include "Types.h"

#include <QList>

namespace FlashSpartan::Policy {

/** In-memory authoritative policy state. */
struct PolicySnapshot {
    QList<DeviceRecord> devices;
    QList<BlockedDriveEntry> blocks;
};

} // namespace FlashSpartan::Policy
