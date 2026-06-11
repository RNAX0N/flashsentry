/**
 * flashspartan-policyd — isolated process owning the signed policy store.
 * All trust/block mutations go through this daemon; the GUI uses PolicyDaemonClient.
 */

#include "policy/PolicyPaths.h"
#include "policy/PolicySocketAuth.h"
#include "policy/PolicyStoreEngine.h"
#include "policy/PolicyAudit.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>

using namespace FlashSpartan;
using namespace FlashSpartan::Policy;

class PolicyDaemonServer : public QObject {
    Q_OBJECT

public:
    explicit PolicyDaemonServer(QObject* parent = nullptr)
        : QObject(parent)
    {
        QString err;
        if (!m_engine.load(&err)) {
            qWarning() << "policyd: load failed:" << err;
        }
        connect(&m_server, &QLocalServer::newConnection, this, &PolicyDaemonServer::onNewConnection);
    }

    ~PolicyDaemonServer() override
    {
        QFile::remove(PolicyPaths::tokenPath());
    }

    bool listen(QString* error)
    {
        QLocalServer::removeServer(PolicyPaths::socketPath());
        if (!m_server.listen(PolicyPaths::socketPath())) {
            if (error) {
                *error = m_server.errorString();
            }
            return false;
        }

        m_authToken = PolicySocketAuth::generateToken();
        if (!PolicySocketAuth::writeTokenFile(PolicyPaths::tokenPath(), m_authToken, error)) {
            m_server.close();
            return false;
        }
        return true;
    }

private slots:
    void onNewConnection()
    {
        while (QLocalSocket* socket = m_server.nextPendingConnection()) {
            if (!PolicySocketAuth::peerMatchesServerUser(socket->socketDescriptor())) {
                socket->write(
                    QJsonDocument(QJsonObject{{QStringLiteral("ok"), false},
                                              {QStringLiteral("error"),
                                               QStringLiteral("unauthorized peer")}})
                        .toJson(QJsonDocument::Compact));
                socket->write("\n");
                socket->flush();
                socket->disconnectFromServer();
                socket->deleteLater();
                continue;
            }

            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                const QByteArray line = socket->readAll().trimmed();
                QJsonObject resp;
                if (line.isEmpty()) {
                    resp = {{QStringLiteral("ok"), false},
                            {QStringLiteral("error"), QStringLiteral("empty request")}};
                } else {
                    const QJsonDocument doc = QJsonDocument::fromJson(line);
                    resp = doc.isObject() ? dispatch(doc.object())
                                          : QJsonObject{{QStringLiteral("ok"), false},
                                                        {QStringLiteral("error"),
                                                         QStringLiteral("invalid json")}};
                }
                socket->write(QJsonDocument(resp).toJson(QJsonDocument::Compact));
                socket->write("\n");
                socket->flush();
                socket->disconnectFromServer();
                socket->deleteLater();
            });
        }
    }

private:
    QJsonObject unauthorized() const
    {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("unauthorized")}};
    }

    bool requiresAuth(const QString& op) const
    {
        return op != QLatin1String("ping");
    }

    QJsonObject dispatch(const QJsonObject& req)
    {
        const QString op = req[QStringLiteral("op")].toString();
        if (requiresAuth(op) && !PolicySocketAuth::requestAuthorized(req, m_authToken)) {
            return unauthorized();
        }

        const QString actor = req.value(QStringLiteral("actor")).toString(QStringLiteral("policyd"));

        if (op == QLatin1String("ping")) {
            return {{QStringLiteral("ok"), true}};
        }

        if (op == QLatin1String("load") || op == QLatin1String("reload")) {
            QString err;
            const bool ok = m_engine.load(&err);
            QJsonObject r{{QStringLiteral("ok"), ok}};
            if (!ok) {
                r[QStringLiteral("error")] = err;
            }
            return r;
        }

        if (op == QLatin1String("snapshot")) {
            const PolicySnapshot snap = m_engine.snapshot();
            QJsonArray devices;
            for (const DeviceRecord& rec : snap.devices) {
                devices.append(rec.toJson());
            }
            QJsonArray blocks;
            for (const BlockedDriveEntry& e : snap.blocks) {
                QJsonObject o;
                o[QStringLiteral("drive_key")] = e.driveKey;
                o[QStringLiteral("unique_id")] = e.uniqueId;
                o[QStringLiteral("label")] = e.label;
                o[QStringLiteral("blocked_at")] = e.blockedAt.toString(Qt::ISODate);
                blocks.append(o);
            }
            return {{QStringLiteral("ok"), true},
                    {QStringLiteral("devices"), devices},
                    {QStringLiteral("blocks"), blocks}};
        }

        if (op == QLatin1String("upsert_device")) {
            DeviceRecord rec = DeviceRecord::fromJson(req[QStringLiteral("record")].toObject());
            return {{QStringLiteral("ok"),
                     m_engine.upsertDevice(rec, actor, req[QStringLiteral("reason")].toString())}};
        }

        if (op == QLatin1String("remove_device")) {
            return {{QStringLiteral("ok"),
                     m_engine.removeDevice(req[QStringLiteral("unique_id")].toString(), actor,
                                           req[QStringLiteral("reason")].toString())}};
        }

        if (op == QLatin1String("clear_devices")) {
            return {{QStringLiteral("ok"),
                     m_engine.clearDevices(actor, req[QStringLiteral("reason")].toString())}};
        }

        if (op == QLatin1String("block_drive")) {
            return {{QStringLiteral("ok"),
                     m_engine.blockDrive(req[QStringLiteral("drive_key")].toString(),
                                         req[QStringLiteral("unique_id")].toString(),
                                         req[QStringLiteral("label")].toString(), actor)}};
        }

        if (op == QLatin1String("unblock_drive")) {
            return {{QStringLiteral("ok"),
                     m_engine.unblockDrive(req[QStringLiteral("drive_key")].toString(),
                                           req[QStringLiteral("unique_id")].toString(), actor)}};
        }

        if (op == QLatin1String("export_json")) {
            const PolicySnapshot snap = m_engine.snapshot();
            QJsonArray arr;
            for (const DeviceRecord& rec : snap.devices) {
                arr.append(rec.toJson());
            }
            QJsonObject root;
            root[QStringLiteral("version")] = QStringLiteral("1.0");
            root[QStringLiteral("devices")] = arr;
            QFile f(req[QStringLiteral("path")].toString());
            const bool ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && f.write(QJsonDocument(root).toJson(
                       req[QStringLiteral("pretty")].toBool() ? QJsonDocument::Indented
                                                              : QJsonDocument::Compact))
                       >= 0;
            return {{QStringLiteral("ok"), ok}};
        }

        if (op == QLatin1String("import_json")) {
            QFile f(req[QStringLiteral("path")].toString());
            QJsonObject r{{QStringLiteral("ok"), false}};
            if (!f.open(QIODevice::ReadOnly)) {
                r[QStringLiteral("error")] = QStringLiteral("cannot open import");
                return r;
            }
            const QJsonArray arr =
                QJsonDocument::fromJson(f.readAll()).object()[QStringLiteral("devices")].toArray();
            if (!req[QStringLiteral("merge")].toBool()) {
                m_engine.clearDevices(actor, QStringLiteral("import replace"));
            }
            int count = 0;
            for (const QJsonValue& v : arr) {
                if (!v.isObject()) {
                    continue;
                }
                DeviceRecord rec = DeviceRecord::fromJson(v.toObject());
                if (!rec.uniqueId.isEmpty()
                    && m_engine.upsertDevice(rec, actor, QStringLiteral("import"))) {
                    ++count;
                }
            }
            return {{QStringLiteral("ok"), true}, {QStringLiteral("count"), count}};
        }

        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("unknown op: %1").arg(op)}};
    }

    QLocalServer m_server;
    PolicyStoreEngine m_engine;
    QString m_authToken;
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flashspartan-policyd"));

    PolicyDaemonServer server;
    QString err;
    if (!server.listen(&err)) {
        qCritical() << "policyd: listen failed:" << err;
        return 1;
    }

    PolicyAudit::append(QStringLiteral("policyd"), QStringLiteral("start"), QStringLiteral("*"),
                        PolicyPaths::socketPath());
    return app.exec();
}

#include "flashspartan-policyd.moc"
