#pragma once

#include "policy/PolicyGateway.h"
#include "policy/PolicyStoreEngine.h"

namespace FlashSpartan::Policy {

class PolicyInProcessGateway : public PolicyGateway {
public:
    explicit PolicyInProcessGateway(QString storePath = {});

    bool load(QString* error = nullptr) override;
    bool reload(QString* error = nullptr) override;
    PolicySnapshot snapshot() const override;

    bool upsertDevice(const DeviceRecord& record, const QString& actor,
                      const QString& reason) override;
    bool removeDevice(const QString& uniqueId, const QString& actor,
                      const QString& reason) override;
    bool clearDevices(const QString& actor, const QString& reason) override;
    bool blockDrive(const QString& driveKey, const QString& uniqueId, const QString& label,
                    const QString& actor) override;
    bool unblockDrive(const QString& driveKey, const QString& uniqueId,
                      const QString& actor) override;

    bool exportJson(const QString& path, bool prettyPrint, QString* error = nullptr) const override;
    int importJson(const QString& path, bool merge, const QString& actor,
                   QString* error = nullptr) override;

    PolicyStoreEngine& engine() { return m_engine; }

private:
    PolicyStoreEngine m_engine;
};

} // namespace FlashSpartan::Policy
