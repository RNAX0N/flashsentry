#pragma once

#include "Types.h"

namespace FlashSpartan {

/** Append-only JSON-lines audit log for verification events. */
class AuditLog {
public:
    static QString logPath();

    static void appendIsoVerify(const IsoVerifyResult& result);
    static void appendBadUsbEvent(const BadUsbAnomalyResult& result);
    static void appendEvent(const QString& event, const QString& detail = {});

private:
    static void appendLine(const QString& jsonLine);
};

} // namespace FlashSpartan
