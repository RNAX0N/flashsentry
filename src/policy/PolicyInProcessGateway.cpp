#include "policy/PolicyInProcessGateway.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace FlashSpartan::Policy {

PolicyInProcessGateway::PolicyInProcessGateway(QString storePath)
    : m_engine(std::move(storePath))
{
}

bool PolicyInProcessGateway::load(QString* error)
{
    return m_engine.load(error);
}

bool PolicyInProcessGateway::reload(QString* error)
{
    return m_engine.load(error);
}

PolicySnapshot PolicyInProcessGateway::snapshot() const
{
    return m_engine.snapshot();
}

bool PolicyInProcessGateway::upsertDevice(const DeviceRecord& record, const QString& actor,
                                          const QString& reason)
{
    return m_engine.upsertDevice(record, actor, reason);
}

bool PolicyInProcessGateway::removeDevice(const QString& uniqueId, const QString& actor,
                                          const QString& reason)
{
    return m_engine.removeDevice(uniqueId, actor, reason);
}

bool PolicyInProcessGateway::clearDevices(const QString& actor, const QString& reason)
{
    return m_engine.clearDevices(actor, reason);
}

bool PolicyInProcessGateway::blockDrive(const QString& driveKey, const QString& uniqueId,
                                        const QString& label, const QString& actor)
{
    return m_engine.blockDrive(driveKey, uniqueId, label, actor);
}

bool PolicyInProcessGateway::unblockDrive(const QString& driveKey, const QString& uniqueId,
                                          const QString& actor)
{
    return m_engine.unblockDrive(driveKey, uniqueId, actor);
}

bool PolicyInProcessGateway::exportJson(const QString& path, bool prettyPrint,
                                        QString* error) const
{
    const PolicySnapshot snap = m_engine.snapshot();
    QJsonArray arr;
    for (const DeviceRecord& rec : snap.devices) {
        arr.append(rec.toJson());
    }
    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("1.0");
    root[QStringLiteral("devices")] = arr;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot write export file");
        }
        return false;
    }
    f.write(QJsonDocument(root).toJson(prettyPrint ? QJsonDocument::Indented
                                                     : QJsonDocument::Compact));
    return true;
}

int PolicyInProcessGateway::importJson(const QString& path, bool merge, const QString& actor,
                                       QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open import file");
        }
        return -1;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid import JSON");
        }
        return -1;
    }

    PolicySnapshot snap = merge ? m_engine.snapshot() : PolicySnapshot{};
    if (!merge) {
        m_engine.clearDevices(actor, QStringLiteral("import replace"));
    }

    int count = 0;
    const QJsonArray arr = doc.object()[QStringLiteral("devices")].toArray();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        DeviceRecord rec = DeviceRecord::fromJson(v.toObject());
        if (rec.uniqueId.isEmpty()) {
            continue;
        }
        if (m_engine.upsertDevice(rec, actor, QStringLiteral("import"))) {
            ++count;
        }
    }
    return count;
}

} // namespace FlashSpartan::Policy
