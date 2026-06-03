#include "AppDiagnostics.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMessageLogContext>
#include <QStandardPaths>
#include <QTextStream>

#include <iostream>

namespace FlashSpartan {

QString AppDiagnostics::logsDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString dir = base + QStringLiteral("/logs");
    QDir().mkpath(dir);
    return dir;
}

QString AppDiagnostics::qtLogPath()
{
    return logsDir() + QStringLiteral("/flashspartan.log");
}

QString AppDiagnostics::hostUsbInventoryPath()
{
    return logsDir() + QStringLiteral("/host-usb-inventory.jsonl");
}

void AppDiagnostics::appendHostUsbInventorySnapshot(const QList<UsbHostDeviceInfo>& devices,
                                                    const QString& trigger)
{
    QJsonObject root;
    root.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!trigger.isEmpty()) {
        root.insert(QStringLiteral("trigger"), trigger);
    }
    root.insert(QStringLiteral("count"), devices.size());

    QJsonArray items;
    for (const UsbHostDeviceInfo& d : devices) {
        QJsonObject o;
        o.insert(QStringLiteral("instance_id"), d.instanceId);
        o.insert(QStringLiteral("name"), d.displayName);
        o.insert(QStringLiteral("category"), d.category);
        o.insert(QStringLiteral("tier"), d.tier == UsbHostTier::InternalHost
                                          ? QStringLiteral("internal")
                                          : QStringLiteral("peripheral"));
        o.insert(QStringLiteral("vid"), d.vendorId);
        o.insert(QStringLiteral("pid"), d.productId);
        o.insert(QStringLiteral("manufacturer"), d.manufacturer);
        items.append(o);
    }
    root.insert(QStringLiteral("devices"), items);

    QFile file(hostUsbInventoryPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream out(&file);
    out << QJsonDocument(root).toJson(QJsonDocument::Compact) << QLatin1Char('\n');
}

void AppDiagnostics::installQtMessageHandler()
{
    static QFile logFile;
    static QTextStream logStream;
    static bool initialized = false;

    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
        Q_UNUSED(context)

        if (!initialized) {
            logFile.setFileName(qtLogPath());
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                logStream.setDevice(&logFile);
                initialized = true;
            }
        }

        QString level;
        switch (type) {
            case QtDebugMsg:
                level = QStringLiteral("DEBUG");
                break;
            case QtInfoMsg:
                level = QStringLiteral("INFO");
                break;
            case QtWarningMsg:
                level = QStringLiteral("WARN");
                break;
            case QtCriticalMsg:
                level = QStringLiteral("ERROR");
                break;
            case QtFatalMsg:
                level = QStringLiteral("FATAL");
                break;
        }

        const QString line = QStringLiteral("[%1] [%3] %2")
                                 .arg(QDateTime::currentDateTime().toString(
                                          QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")),
                                      msg,
                                      level.leftJustified(5, QLatin1Char(' ')));

        std::cerr << line.toStdString() << std::endl;
        if (initialized) {
            logStream << line << QLatin1Char('\n');
            logStream.flush();
        }

        if (type == QtFatalMsg) {
            abort();
        }
    });
}

} // namespace FlashSpartan
