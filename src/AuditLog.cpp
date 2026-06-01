#include "AuditLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace FlashSpartan {

QString AuditLog::logPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/audit.log");
}

void AuditLog::appendLine(const QString& jsonLine)
{
    QFile f(logPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    f.write(jsonLine.toUtf8());
    f.write("\n");
}

void AuditLog::appendEvent(const QString& event, const QString& detail)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("ts"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    obj.insert(QStringLiteral("event"), event);
    if (!detail.isEmpty()) {
        obj.insert(QStringLiteral("detail"), detail);
    }
    appendLine(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void AuditLog::appendIsoVerify(const IsoVerifyResult& result)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("ts"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    obj.insert(QStringLiteral("event"), QStringLiteral("iso_verify"));
    obj.insert(QStringLiteral("path"), result.isoPath);
    obj.insert(QStringLiteral("device"), result.deviceNode);
    obj.insert(QStringLiteral("publisher"), result.publisherId);
    obj.insert(QStringLiteral("passed"), result.passed());
    obj.insert(QStringLiteral("hash_matches"), result.hashMatches);
    obj.insert(QStringLiteral("pgp_valid"), result.pgpValid);
    obj.insert(QStringLiteral("source"), static_cast<int>(result.source));
    if (!result.errorMessage.isEmpty()) {
        obj.insert(QStringLiteral("error"), result.errorMessage);
    }
    appendLine(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void AuditLog::appendBadUsbEvent(const BadUsbAnomalyResult& result)
{
    QJsonObject obj = result.toJson();
    obj.insert(QStringLiteral("ts"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    obj.insert(QStringLiteral("event"), QStringLiteral("badusb_anomaly"));
    appendLine(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

} // namespace FlashSpartan
