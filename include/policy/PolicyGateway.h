#pragma once

#include "PolicySnapshot.h"
#include "Types.h"

#include <functional>
#include <memory>
#include <optional>

namespace FlashSpartan::Policy {

/**
 * Sole gate for policy mutations. UI and monitors must not touch policy files.
 */
class PolicyGateway {
public:
    virtual ~PolicyGateway() = default;

    virtual bool load(QString* error = nullptr) = 0;
    virtual bool reload(QString* error = nullptr) = 0;

    virtual PolicySnapshot snapshot() const = 0;

    virtual bool upsertDevice(const DeviceRecord& record, const QString& actor,
                              const QString& reason) = 0;
    virtual bool removeDevice(const QString& uniqueId, const QString& actor,
                              const QString& reason) = 0;
    virtual bool clearDevices(const QString& actor, const QString& reason) = 0;
    virtual bool blockDrive(const QString& driveKey, const QString& uniqueId,
                            const QString& label, const QString& actor) = 0;
    virtual bool unblockDrive(const QString& driveKey, const QString& uniqueId,
                              const QString& actor) = 0;

    virtual bool exportJson(const QString& path, bool prettyPrint, QString* error = nullptr) const = 0;
    virtual int importJson(const QString& path, bool merge, const QString& actor,
                           QString* error = nullptr) = 0;

    static std::unique_ptr<PolicyGateway> createDefault();
    static std::unique_ptr<PolicyGateway> createInProcess(const QString& storePath = {});
};

} // namespace FlashSpartan::Policy
