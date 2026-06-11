#pragma once

#include "Types.h"

namespace FlashSpartan {

class DatabaseManager;

/** Builds trusted-device records and identity warnings for the USB whitelist flow. */
class DeviceWhitelistService {
public:
    static DeviceRecord makeRecord(const DeviceInfo& device, const DatabaseManager& database,
                                   int trustLevel);

    /** HTML paragraph for trust dialogs when serial is missing; empty when identity is strong. */
    static QString weakIdentityNoticeHtml(const DeviceInfo& device);
};

} // namespace FlashSpartan
