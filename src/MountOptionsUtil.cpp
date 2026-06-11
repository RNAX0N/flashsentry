#include "MountOptionsUtil.h"

namespace FlashSpartan {

QVariantMap MountOptionsUtil::toUdisksMountOptions(const MountOptions& options)
{
    QVariantMap map;

    QStringList optionsList;
    if (options.readOnly) {
        optionsList << QStringLiteral("ro");
    }
    if (options.noExec) {
        optionsList << QStringLiteral("noexec");
    }
    if (options.noSuid) {
        optionsList << QStringLiteral("nosuid");
    }
    if (options.sync) {
        optionsList << QStringLiteral("sync");
    }
    optionsList << options.extraOptions;

    if (!optionsList.isEmpty()) {
        map.insert(QStringLiteral("options"), optionsList.join(QLatin1Char(',')));
    }
    if (!options.filesystem.isEmpty()) {
        map.insert(QStringLiteral("fstype"), options.filesystem);
    }
    return map;
}

QVariantMap MountOptionsUtil::toUdisksUnmountOptions(const UnmountOptions& options)
{
    QVariantMap map;
    if (options.force) {
        map.insert(QStringLiteral("force"), true);
    }
    return map;
}

} // namespace FlashSpartan
