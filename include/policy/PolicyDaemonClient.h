#pragma once

#include "policy/PolicyGateway.h"

namespace FlashSentry::Policy {

class PolicyDaemonClient : public PolicyGateway {
public:
    explicit PolicyDaemonClient(QString socketPath = {});

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

    bool ping(QString* error = nullptr) const;

private:
    QJsonObject request(const QJsonObject& req, QString* error = nullptr) const;

    QString m_socketPath;
};

} // namespace FlashSentry::Policy
