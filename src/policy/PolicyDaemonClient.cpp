#include "policy/PolicyDaemonClient.h"
#include "policy/PolicyPaths.h"

#include <QLocalSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace FlashSpartan::Policy {

PolicyDaemonClient::PolicyDaemonClient(QString socketPath)
    : m_socketPath(socketPath.isEmpty() ? PolicyPaths::socketPath() : std::move(socketPath))
{
}

QJsonObject PolicyDaemonClient::request(const QJsonObject& req, QString* error) const
{
    QLocalSocket socket;
    socket.connectToServer(m_socketPath);
    if (!socket.waitForConnected(3000)) {
        if (error) {
            *error = QStringLiteral("Cannot connect to flashspartan-policyd");
        }
        return {};
    }

    const QByteArray line = QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n';
    socket.write(line);
    socket.flush();
    if (!socket.waitForBytesWritten(3000)) {
        if (error) {
            *error = QStringLiteral("Policy daemon write timeout");
        }
        return {};
    }

    if (!socket.waitForReadyRead(10000)) {
        if (error) {
            *error = QStringLiteral("Policy daemon read timeout");
        }
        return {};
    }

    const QByteArray resp = socket.readAll();
    socket.disconnectFromServer();

    const QJsonDocument doc = QJsonDocument::fromJson(resp.trimmed());
    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid policy daemon response");
        }
        return {};
    }
    return doc.object();
}

bool PolicyDaemonClient::ping(QString* error) const
{
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("ping");
    const QJsonObject resp = request(req, error);
    return resp.value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::load(QString* error)
{
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("load");
    const QJsonObject resp = request(req, error);
    return resp.value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::reload(QString* error)
{
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("reload");
    const QJsonObject resp = request(req, error);
    return resp.value(QStringLiteral("ok")).toBool();
}

PolicySnapshot PolicyDaemonClient::snapshot() const
{
    PolicySnapshot snap;
    QString err;
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("snapshot");
    const QJsonObject resp = request(req, &err);
    if (!resp.value(QStringLiteral("ok")).toBool()) {
        return snap;
    }

    const QJsonArray devices = resp[QStringLiteral("devices")].toArray();
    for (const QJsonValue& v : devices) {
        if (v.isObject()) {
            snap.devices.append(DeviceRecord::fromJson(v.toObject()));
        }
    }
    const QJsonArray blocks = resp[QStringLiteral("blocks")].toArray();
    for (const QJsonValue& v : blocks) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        BlockedDriveEntry e;
        e.driveKey = o[QStringLiteral("drive_key")].toString();
        e.uniqueId = o[QStringLiteral("unique_id")].toString();
        e.label = o[QStringLiteral("label")].toString();
        e.blockedAt = QDateTime::fromString(o[QStringLiteral("blocked_at")].toString(),
                                            Qt::ISODate);
        snap.blocks.append(e);
    }
    return snap;
}

bool PolicyDaemonClient::upsertDevice(const DeviceRecord& record, const QString& actor,
                                      const QString& reason)
{
    QString err;
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("upsert_device");
    req[QStringLiteral("actor")] = actor;
    req[QStringLiteral("reason")] = reason;
    req[QStringLiteral("record")] = record.toJson();
    return request(req, &err).value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::removeDevice(const QString& uniqueId, const QString& actor,
                                      const QString& reason)
{
    QString err;
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("remove_device");
    req[QStringLiteral("actor")] = actor;
    req[QStringLiteral("reason")] = reason;
    req[QStringLiteral("unique_id")] = uniqueId;
    return request(req, &err).value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::clearDevices(const QString& actor, const QString& reason)
{
    QString err;
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("clear_devices");
    req[QStringLiteral("actor")] = actor;
    req[QStringLiteral("reason")] = reason;
    return request(req, &err).value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::blockDrive(const QString& driveKey, const QString& uniqueId,
                                    const QString& label, const QString& actor)
{
    QString err;
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("block_drive");
    req[QStringLiteral("actor")] = actor;
    req[QStringLiteral("drive_key")] = driveKey;
    req[QStringLiteral("unique_id")] = uniqueId;
    req[QStringLiteral("label")] = label;
    return request(req, &err).value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::unblockDrive(const QString& driveKey, const QString& uniqueId,
                                      const QString& actor)
{
    QString err;
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("unblock_drive");
    req[QStringLiteral("actor")] = actor;
    req[QStringLiteral("drive_key")] = driveKey;
    req[QStringLiteral("unique_id")] = uniqueId;
    return request(req, &err).value(QStringLiteral("ok")).toBool();
}

bool PolicyDaemonClient::exportJson(const QString& path, bool prettyPrint, QString* error) const
{
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("export_json");
    req[QStringLiteral("path")] = path;
    req[QStringLiteral("pretty")] = prettyPrint;
    return request(req, error).value(QStringLiteral("ok")).toBool();
}

int PolicyDaemonClient::importJson(const QString& path, bool merge, const QString& actor,
                                   QString* error)
{
    QJsonObject req;
    req[QStringLiteral("op")] = QStringLiteral("import_json");
    req[QStringLiteral("path")] = path;
    req[QStringLiteral("merge")] = merge;
    req[QStringLiteral("actor")] = actor;
    const QJsonObject resp = request(req, error);
    if (!resp.value(QStringLiteral("ok")).toBool()) {
        return -1;
    }
    return resp.value(QStringLiteral("count")).toInt(-1);
}

} // namespace FlashSpartan::Policy
