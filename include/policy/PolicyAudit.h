#pragma once

#include <QString>

namespace FlashSentry::Policy {

/** Append-only audit trail written only by the policy engine / daemon. */
class PolicyAudit {
public:
    static void append(const QString& actor, const QString& action,
                       const QString& targetId, const QString& detail = {});

private:
    static void appendLine(const QString& jsonLine);
};

} // namespace FlashSentry::Policy
