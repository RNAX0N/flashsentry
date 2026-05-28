#include "HashCheckpoint.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace FlashSentry {

HashCheckpointStore& HashCheckpointStore::instance()
{
    static HashCheckpointStore store;
    return store;
}

static QString checkpointPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/FlashSentry");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/hash-checkpoints.json");
}

void HashCheckpointStore::load()
{
    m_checkpoints.clear();
    QFile file(checkpointPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }
    for (const QJsonValue& v : doc.object().value(QStringLiteral("checkpoints")).toArray()) {
        const QJsonObject o = v.toObject();
        HashCheckpoint cp;
        cp.deviceNode = o.value(QStringLiteral("device_node")).toString();
        cp.algorithm = o.value(QStringLiteral("algorithm")).toString();
        cp.scanMode = o.value(QStringLiteral("scan_mode")).toString(QStringLiteral("full"));
        cp.deviceSize = static_cast<uint64_t>(o.value(QStringLiteral("device_size")).toDouble());
        cp.blockSize = static_cast<uint64_t>(o.value(QStringLiteral("block_size")).toDouble());
        cp.bytesCompleted = static_cast<uint64_t>(o.value(QStringLiteral("bytes_completed")).toDouble());
        for (const QJsonValue& hv : o.value(QStringLiteral("block_hashes")).toArray()) {
            cp.blockHashes.append(hv.toString());
        }
        if (cp.isValid()) {
            m_checkpoints.append(cp);
        }
    }
}

void HashCheckpointStore::save()
{
    QJsonArray arr;
    for (const HashCheckpoint& cp : m_checkpoints) {
        QJsonObject o;
        o.insert(QStringLiteral("device_node"), cp.deviceNode);
        o.insert(QStringLiteral("algorithm"), cp.algorithm);
        o.insert(QStringLiteral("scan_mode"), cp.scanMode);
        o.insert(QStringLiteral("device_size"), static_cast<double>(cp.deviceSize));
        o.insert(QStringLiteral("block_size"), static_cast<double>(cp.blockSize));
        o.insert(QStringLiteral("bytes_completed"), static_cast<double>(cp.bytesCompleted));
        QJsonArray hashes;
        for (const QString& h : cp.blockHashes) {
            hashes.append(h);
        }
        o.insert(QStringLiteral("block_hashes"), hashes);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), QStringLiteral("1.0"));
    root.insert(QStringLiteral("checkpoints"), arr);

    QSaveFile file(checkpointPath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

std::optional<HashCheckpoint> HashCheckpointStore::checkpointFor(const QString& deviceNode,
                                                                   const QString& algorithm,
                                                                   const QString& scanMode) const
{
    for (const HashCheckpoint& cp : m_checkpoints) {
        if (cp.deviceNode == deviceNode && cp.algorithm == algorithm && cp.scanMode == scanMode) {
            return cp;
        }
    }
    return std::nullopt;
}

void HashCheckpointStore::upsert(const HashCheckpoint& cp)
{
    remove(cp.deviceNode, cp.algorithm, cp.scanMode);
    m_checkpoints.append(cp);
    save();
}

void HashCheckpointStore::remove(const QString& deviceNode, const QString& algorithm,
                                 const QString& scanMode)
{
    m_checkpoints.erase(
        std::remove_if(m_checkpoints.begin(), m_checkpoints.end(),
                       [&](const HashCheckpoint& c) {
                           return c.deviceNode == deviceNode && c.algorithm == algorithm
                               && c.scanMode == scanMode;
                       }),
        m_checkpoints.end());
    save();
}

void HashCheckpointStore::clearAll()
{
    m_checkpoints.clear();
    save();
}

} // namespace FlashSentry
