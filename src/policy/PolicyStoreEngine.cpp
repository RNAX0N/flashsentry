#include "policy/PolicyStoreEngine.h"

#include "policy/PolicyAudit.h"
#include "policy/PolicyBlobCodec.h"
#include "policy/PolicyPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace FlashSpartan::Policy {

namespace {

bool writeStoreFile(const QString& path, const PolicySnapshot& snapshot, QString* error)
{
    const QByteArray payload = PolicyBlobCodec::encode(snapshot);
    const QByteArray key = PolicyBlobCodec::loadOrCreateKey();
    const QByteArray sig = PolicyBlobCodec::sign(payload, key);

    QByteArray file;
    QDataStream out(&file, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << PolicyBlobCodec::kMagic << PolicyBlobCodec::kVersion << quint32(payload.size());
    out.writeRawData(payload.constData(), payload.size());
    out.writeRawData(sig.constData(), sig.size());

    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot open policy store for writing");
        }
        return false;
    }
    sf.write(file);
    if (!sf.commit()) {
        if (error) {
            *error = QStringLiteral("Failed to commit policy store");
        }
        return false;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

bool readStoreFile(const QString& path, PolicySnapshot& snapshot, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Policy store not found");
        }
        return false;
    }
    return PolicyBlobCodec::decode(f.readAll(), snapshot, error);
}

} // namespace

PolicyStoreEngine::PolicyStoreEngine(QString storePath)
    : m_storePath(storePath.isEmpty() ? PolicyPaths::storeFilePath() : std::move(storePath))
{
}

bool PolicyStoreEngine::load(QString* error)
{
    QWriteLocker locker(&m_lock);
    QDir().mkpath(QFileInfo(m_storePath).absolutePath());

    if (!QFile::exists(m_storePath)) {
        migrateLegacyJsonIfNeeded(error);
        if (!QFile::exists(m_storePath)) {
            m_snapshot = {};
            return writeStoreFile(m_storePath, m_snapshot, error);
        }
    }

    return readStoreFile(m_storePath, m_snapshot, error);
}

bool PolicyStoreEngine::save(QString* error)
{
    QReadLocker locker(&m_lock);
    return writeStoreFile(m_storePath, m_snapshot, error);
}

PolicySnapshot PolicyStoreEngine::snapshot() const
{
    QReadLocker locker(&m_lock);
    return m_snapshot;
}

void PolicyStoreEngine::setSnapshot(const PolicySnapshot& snap)
{
    QWriteLocker locker(&m_lock);
    m_snapshot = snap;
}

bool PolicyStoreEngine::migrateLegacyJsonIfNeeded(QString* error)
{
    const QString legacy = PolicyPaths::legacyDevicesJsonPath();
    const QString legacyBlocks = PolicyPaths::legacyBlockedJsonPath();

    PolicySnapshot snap;
    bool migrated = false;

    if (QFile::exists(legacy)) {
        QFile f(legacy);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            const QJsonArray arr = doc.object()[QStringLiteral("devices")].toArray();
            for (const QJsonValue& v : arr) {
                if (v.isObject()) {
                    DeviceRecord rec = DeviceRecord::fromJson(v.toObject());
                    if (!rec.uniqueId.isEmpty()) {
                        snap.devices.append(rec);
                    }
                }
            }
            migrated = true;
            f.close();
            QFile::rename(legacy, legacy + QStringLiteral(".migrated"));
        }
    }

    if (QFile::exists(legacyBlocks)) {
        QFile f(legacyBlocks);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonArray arr =
                QJsonDocument::fromJson(f.readAll()).object()[QStringLiteral("entries")].toArray();
            for (const QJsonValue& v : arr) {
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
            QFile::rename(legacyBlocks, legacyBlocks + QStringLiteral(".migrated"));
            migrated = true;
        }
    }

    if (!migrated) {
        return true;
    }

    m_snapshot = snap;
    PolicyAudit::append(QStringLiteral("engine"), QStringLiteral("migrate"),
                        QStringLiteral("*"), QStringLiteral("Imported legacy JSON stores"));
    return writeStoreFile(m_storePath, m_snapshot, error);
}

bool PolicyStoreEngine::commitMutation(const QString& actor, const QString& action,
                                       const QString& target, const QString& detail)
{
    PolicyAudit::append(actor, action, target, detail);
    QString err;
    if (!save(&err)) {
        return false;
    }
    return true;
}

bool PolicyStoreEngine::upsertDevice(const DeviceRecord& record, const QString& actor,
                                    const QString& reason)
{
    if (record.uniqueId.isEmpty()) {
        return false;
    }
    {
        QWriteLocker locker(&m_lock);
        bool found = false;
        for (DeviceRecord& rec : m_snapshot.devices) {
            if (rec.uniqueId == record.uniqueId) {
                rec = record;
                found = true;
                break;
            }
        }
        if (!found) {
            m_snapshot.devices.append(record);
        }
    }
    return commitMutation(actor, QStringLiteral("upsert_device"), record.uniqueId, reason);
}

bool PolicyStoreEngine::removeDevice(const QString& uniqueId, const QString& actor,
                                     const QString& reason)
{
    {
        QWriteLocker locker(&m_lock);
        QList<DeviceRecord> kept;
        for (const DeviceRecord& rec : m_snapshot.devices) {
            if (rec.uniqueId != uniqueId) {
                kept.append(rec);
            }
        }
        m_snapshot.devices = kept;
    }
    return commitMutation(actor, QStringLiteral("remove_device"), uniqueId, reason);
}

bool PolicyStoreEngine::clearDevices(const QString& actor, const QString& reason)
{
    {
        QWriteLocker locker(&m_lock);
        m_snapshot.devices.clear();
    }
    return commitMutation(actor, QStringLiteral("clear_devices"), QStringLiteral("*"), reason);
}

bool PolicyStoreEngine::blockDrive(const QString& driveKey, const QString& uniqueId,
                                  const QString& label, const QString& actor)
{
    {
        QWriteLocker locker(&m_lock);
        for (const BlockedDriveEntry& e : m_snapshot.blocks) {
            if ((!driveKey.isEmpty() && e.driveKey == driveKey)
                || (!uniqueId.isEmpty() && e.uniqueId == uniqueId)) {
                return commitMutation(actor, QStringLiteral("block_drive"), uniqueId, label);
            }
        }
        BlockedDriveEntry e;
        e.driveKey = driveKey;
        e.uniqueId = uniqueId;
        e.label = label;
        e.blockedAt = QDateTime::currentDateTime();
        m_snapshot.blocks.append(e);
    }
    return commitMutation(actor, QStringLiteral("block_drive"), uniqueId, label);
}

bool PolicyStoreEngine::unblockDrive(const QString& driveKey, const QString& uniqueId,
                                     const QString& actor)
{
    {
        QWriteLocker locker(&m_lock);
        QList<BlockedDriveEntry> kept;
        for (const BlockedDriveEntry& e : m_snapshot.blocks) {
            const bool match = (!driveKey.isEmpty() && e.driveKey == driveKey)
                               || (!uniqueId.isEmpty() && e.uniqueId == uniqueId);
            if (!match) {
                kept.append(e);
            }
        }
        m_snapshot.blocks = kept;
    }
    return commitMutation(actor, QStringLiteral("unblock_drive"), uniqueId, {});
}

} // namespace FlashSpartan::Policy
