#include "VerifyHistory.h"

#include <QtGlobal>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace FlashSentry {

namespace {

QString historyFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + QStringLiteral("/FlashSentry");
    return dir + QStringLiteral("/verify-history.json");
}

QString kindToString(VerifyHistoryKind kind)
{
    switch (kind) {
        case VerifyHistoryKind::Manifest:
            return QStringLiteral("manifest");
        case VerifyHistoryKind::IsoScan:
            return QStringLiteral("iso");
        case VerifyHistoryKind::Hash:
        default:
            return QStringLiteral("hash");
    }
}

VerifyHistoryKind kindFromString(const QString& s)
{
    if (s == QStringLiteral("manifest")) {
        return VerifyHistoryKind::Manifest;
    }
    if (s == QStringLiteral("iso")) {
        return VerifyHistoryKind::IsoScan;
    }
    return VerifyHistoryKind::Hash;
}

QJsonObject entryToJson(const VerifyHistoryEntry& e)
{
    QJsonObject o;
    o[QStringLiteral("at")] = e.timestamp.toString(Qt::ISODate);
    o[QStringLiteral("device_node")] = e.deviceNode;
    o[QStringLiteral("device_label")] = e.deviceLabel;
    o[QStringLiteral("mount_point")] = e.mountPoint;
    o[QStringLiteral("kind")] = kindToString(e.kind);
    o[QStringLiteral("status")] = e.status;
    o[QStringLiteral("summary")] = e.summary;
    o[QStringLiteral("detail")] = e.detail;
    o[QStringLiteral("duration_ms")] = static_cast<double>(e.durationMs);
    return o;
}

VerifyHistoryEntry entryFromJson(const QJsonObject& o)
{
    VerifyHistoryEntry e;
    e.timestamp = QDateTime::fromString(o[QStringLiteral("at")].toString(), Qt::ISODate);
    if (!e.timestamp.isValid()) {
        e.timestamp = QDateTime::currentDateTimeUtc();
    }
    e.deviceNode = o[QStringLiteral("device_node")].toString();
    e.deviceLabel = o[QStringLiteral("device_label")].toString();
    e.mountPoint = o[QStringLiteral("mount_point")].toString();
    e.kind = kindFromString(o[QStringLiteral("kind")].toString());
    e.status = o[QStringLiteral("status")].toString();
    e.summary = o[QStringLiteral("summary")].toString();
    e.detail = o[QStringLiteral("detail")].toString();
    e.durationMs = static_cast<uint64_t>(o[QStringLiteral("duration_ms")].toDouble());
    return e;
}

} // namespace

VerifyHistory& VerifyHistory::instance()
{
    static VerifyHistory inst;
    return inst;
}

void VerifyHistory::load()
{
    m_entries.clear();
    QFile f(historyFilePath());
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

void VerifyHistory::save()
{
    QJsonArray arr;
    for (const VerifyHistoryEntry& e : m_entries) {
        arr.append(entryToJson(e));
    }
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("entries")] = arr;

    const QString path = historyFilePath();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
}

void VerifyHistory::append(const VerifyHistoryEntry& entry)
{
    VerifyHistoryEntry e = entry;
    if (!e.timestamp.isValid()) {
        e.timestamp = QDateTime::currentDateTimeUtc();
    }
    m_entries.prepend(e);
    while (m_entries.size() > kMaxEntries) {
        m_entries.removeLast();
    }
    save();
}

QList<VerifyHistoryEntry> VerifyHistory::recentEntries(int limit) const
{
    QList<VerifyHistoryEntry> out;
    const int n = qMin(limit, m_entries.size());
    for (int i = 0; i < n; ++i) {
        out.append(m_entries.at(i));
    }
    return out;
}

QList<VerifyHistoryEntry> VerifyHistory::entriesForDevice(const QString& deviceNode,
                                                          int limit) const
{
    QList<VerifyHistoryEntry> out;
    for (const VerifyHistoryEntry& e : m_entries) {
        if (e.deviceNode == deviceNode) {
            out.append(e);
            if (out.size() >= limit) {
                break;
            }
        }
    }
    return out;
}

QString VerifyHistory::formatEntryLine(const VerifyHistoryEntry& entry) const
{
    QString kindLabel;
    switch (entry.kind) {
        case VerifyHistoryKind::Manifest:
            kindLabel = QStringLiteral("Watch");
            break;
        case VerifyHistoryKind::IsoScan:
            kindLabel = QStringLiteral("ISO");
            break;
        case VerifyHistoryKind::Hash:
        default:
            kindLabel = QStringLiteral("Hash");
            break;
    }

    const QString time = entry.timestamp.toLocalTime().toString(QStringLiteral("hh:mm"));
    const QString name = entry.deviceLabel.isEmpty() ? entry.deviceNode : entry.deviceLabel;
    QString line = QStringLiteral("[%1] %2 %3 — %4").arg(time, kindLabel, entry.status, name);
    if (!entry.summary.isEmpty()) {
        line += QStringLiteral(": ") + entry.summary;
    }
    return line;
}

} // namespace FlashSentry
