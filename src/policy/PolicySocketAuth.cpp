#include "policy/PolicySocketAuth.h"

#include <QFile>
#include <QRandomGenerator>

#ifndef Q_OS_WIN
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace FlashSpartan::Policy {

QString PolicySocketAuth::generateToken()
{
    static constexpr int kBytes = 32;
    QByteArray bytes;
    bytes.resize(kBytes);
    for (int i = 0; i < kBytes; ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return QString::fromLatin1(bytes.toHex());
}

bool PolicySocketAuth::writeTokenFile(const QString& path, const QString& token, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot write policy auth token");
        }
        return false;
    }
    file.write(token.toUtf8());
    file.write("\n");
    file.close();
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QString PolicySocketAuth::readTokenFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool PolicySocketAuth::requestAuthorized(const QJsonObject& req, const QString& expectedToken)
{
    if (expectedToken.isEmpty()) {
        return false;
    }
    const QString provided = req.value(QStringLiteral("auth")).toString();
    return !provided.isEmpty() && provided == expectedToken;
}

bool PolicySocketAuth::peerMatchesServerUser(qintptr socketDescriptor)
{
#ifndef Q_OS_WIN
    if (socketDescriptor == -1) {
        return false;
    }
    ucred cred{};
    socklen_t len = sizeof(cred);
    if (getsockopt(static_cast<int>(socketDescriptor), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
        return false;
    }
    return cred.uid == getuid();
#else
    Q_UNUSED(socketDescriptor);
    return true;
#endif
}

} // namespace FlashSpartan::Policy
