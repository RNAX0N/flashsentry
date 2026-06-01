#include "DeviceTimelineLog.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

namespace FlashSpartan {

namespace {

QString timelineFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + QStringLiteral("/FlashSpartan");
    return dir + QStringLiteral("/device-timeline.json");
}

QJsonObject entryToJson(const UiEventEntry& e)
{
    QJsonObject o;
    o[QStringLiteral("id")] = e.id;
    o[QStringLiteral("at")] = e.time.toString(Qt::ISODate);
    o[QStringLiteral("event")] = e.event;
    o[QStringLiteral("device")] = e.device;
    o[QStringLiteral("type")] = e.type;
    o[QStringLiteral("result")] = e.result;
    o[QStringLiteral("detail")] = e.detail;
    o[QStringLiteral("device_node")] = e.deviceNode;
    return o;
}

UiEventEntry entryFromJson(const QJsonObject& o)
{
    UiEventEntry e;
    e.id = o[QStringLiteral("id")].toString();
    e.time = QDateTime::fromString(o[QStringLiteral("at")].toString(), Qt::ISODate);
    if (!e.time.isValid()) {
        e.time = QDateTime::currentDateTimeUtc();
    }
    e.event = o[QStringLiteral("event")].toString();
    e.device = o[QStringLiteral("device")].toString();
    e.type = o[QStringLiteral("type")].toString();
    e.result = o[QStringLiteral("result")].toString();
    e.detail = o[QStringLiteral("detail")].toString();
    e.deviceNode = o[QStringLiteral("device_node")].toString();
    return e;
}

} // namespace

DeviceTimelineLog& DeviceTimelineLog::instance()
{
    static DeviceTimelineLog inst;
    return inst;
}

void DeviceTimelineLog::load()
{
    m_entries.clear();
    QFile f(timelineFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonArray arr = doc.object()[QStringLiteral("entries")].toArray();
    for (const QJsonValue& v : arr) {
        if (v.isObject()) {
            m_entries.append(entryFromJson(v.toObject()));
        }
    }
}

void DeviceTimelineLog::save()
{
    const QString path = timelineFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const UiEventEntry& e : m_entries) {
        arr.append(entryToJson(e));
    }
    QJsonObject root;
    root[QStringLiteral("entries")] = arr;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void DeviceTimelineLog::append(const UiEventEntry& entry)
{
    UiEventEntry e = entry;
    if (e.id.isEmpty()) {
        e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!e.time.isValid()) {
        e.time = QDateTime::currentDateTime();
    }
    m_entries.prepend(e);
    while (m_entries.size() > kMaxTotalEntries) {
        m_entries.removeLast();
    }
    save();
}

QList<UiEventEntry> DeviceTimelineLog::entriesForDevice(const QString& deviceNode,
                                                      int retentionDays,
                                                      int maxEntries) const
{
    QList<UiEventEntry> out;
    const QDateTime cutoff = retentionDays > 0
                                 ? QDateTime::currentDateTime().addDays(-retentionDays)
                                 : QDateTime();

    for (const UiEventEntry& e : m_entries) {
        if (!deviceNode.isEmpty() && e.deviceNode != deviceNode) {
            continue;
        }
        if (retentionDays > 0 && e.time < cutoff) {
            continue;
        }
        out.append(e);
        if (maxEntries > 0 && out.size() >= maxEntries) {
            break;
        }
    }
    return out;
}

QStringList DeviceTimelineLog::knownDeviceNodes() const
{
    QStringList nodes;
    for (const UiEventEntry& e : m_entries) {
        if (!e.deviceNode.isEmpty() && !nodes.contains(e.deviceNode)) {
            nodes.append(e.deviceNode);
        }
    }
    return nodes;
}

} // namespace FlashSpartan
