#include "policy/PolicyAudit.h"
#include "policy/PolicyPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace FlashSpartan::Policy {

void PolicyAudit::appendLine(const QString& jsonLine)
{
    const QString path = PolicyPaths::auditLogPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    f.write(jsonLine.toUtf8());
    f.write("\n");
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
}

void PolicyAudit::append(const QString& actor, const QString& action, const QString& targetId,
                         const QString& detail)
{
    QJsonObject obj;
    obj[QStringLiteral("ts")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    obj[QStringLiteral("actor")] = actor;
    obj[QStringLiteral("action")] = action;
    obj[QStringLiteral("target")] = targetId;
    if (!detail.isEmpty()) {
        obj[QStringLiteral("detail")] = detail;
    }
    appendLine(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

} // namespace FlashSpartan::Policy
