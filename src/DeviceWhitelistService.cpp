#include "DeviceWhitelistService.h"

#include "DatabaseManager.h"

namespace FlashSpartan {

DeviceRecord DeviceWhitelistService::makeRecord(const DeviceInfo& device,
                                                const DatabaseManager& database, int trustLevel)
{
    DeviceRecord record;
    record.uniqueId = database.canonicalUniqueId(device);
    record.firstSeen = QDateTime::currentDateTime();
    record.lastSeen = record.firstSeen;
    record.trustLevel = trustLevel;
    record.lastKnownInfo = device;
    return record;
}

QString DeviceWhitelistService::weakIdentityNoticeHtml(const DeviceInfo& device)
{
    if (!device.hasWeakIdentity()) {
        return {};
    }
    return QStringLiteral(
        "<p style='color:#c90;'><b>Identity note:</b> %1</p>")
        .arg(device.weakIdentitySummary().toHtmlEscaped());
}

} // namespace FlashSpartan
