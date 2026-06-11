#pragma once

#include <QJsonObject>
#include <QString>

namespace FlashSpartan::Policy {

/** Session token auth for flashspartan-policyd local socket IPC. */
class PolicySocketAuth {
public:
    static QString generateToken();
    static bool writeTokenFile(const QString& path, const QString& token, QString* error = nullptr);
    static QString readTokenFile(const QString& path);
    static bool requestAuthorized(const QJsonObject& req, const QString& expectedToken);
    static bool peerMatchesServerUser(qintptr socketDescriptor);
};

} // namespace FlashSpartan::Policy
