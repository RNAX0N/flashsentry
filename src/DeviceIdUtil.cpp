#include "DeviceIdUtil.h"

namespace FlashSpartan {

bool DeviceIdUtil::isPartitionNodeSuffix(const QString& tail)
{
    if (tail.startsWith(QLatin1String("sd")) || tail.startsWith(QLatin1String("mmcblk"))
        || tail.startsWith(QLatin1String("nvme"))) {
        return true;
    }
    return tail.size() == 1 && tail.at(0).isLetter();
}

std::optional<QString> DeviceIdUtil::resolveStoredId(const QHash<QString, DeviceRecord>& devices,
                                                     const QString& uniqueId)
{
    if (devices.contains(uniqueId)) {
        return uniqueId;
    }

    const int sep = uniqueId.lastIndexOf(QLatin1Char('_'));
    if (sep <= 0) {
        return std::nullopt;
    }

    const QString tail = uniqueId.mid(sep + 1);
    if (isPartitionNodeSuffix(tail)) {
        const QString legacy = uniqueId.left(sep);
        if (devices.contains(legacy)) {
            return legacy;
        }
    }

    return std::nullopt;
}

} // namespace FlashSpartan
