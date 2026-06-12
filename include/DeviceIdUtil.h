#pragma once

#include "Types.h"

#include <QHash>
#include <optional>

namespace FlashSpartan {

class DeviceIdUtil {
public:
    static bool isPartitionNodeSuffix(const QString& tail);

    static std::optional<QString> resolveStoredId(const QHash<QString, DeviceRecord>& devices,
                                                  const QString& uniqueId);
};

} // namespace FlashSpartan
