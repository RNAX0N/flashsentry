#include "DatabaseManager.h"
#include "DeviceIdUtil.h"
#include "policy/PolicyGateway.h"
#include "policy/PolicyPaths.h"
#include "policy/PolicyServiceLocator.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>
#include <QDateTime>

namespace FlashSpartan {

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    if (m_modified && m_initialized) {
        save();
    }
}

bool DatabaseManager::initialize(const QString& path)
{
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        const QString configDir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        qputenv("FLASHSPARTAN_POLICY_CONFIG", QDir::toNativeSeparators(configDir).toUtf8());
    }

    if (!Policy::PolicyServiceLocator::hasGateway()) {
        Policy::PolicyServiceLocator::install(Policy::PolicyGateway::createDefault());
    }

    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    if (!gate) {
        emit databaseError(QStringLiteral("Policy gateway unavailable"));
        return false;
    }

    QString err;
    if (!gate->load(&err)) {
        emit databaseError(err.isEmpty() ? QStringLiteral("Policy load failed") : err);
        return false;
    }

    {
        QWriteLocker locker(&m_lock);
        m_databasePath = Policy::PolicyPaths::storeFilePath();
        m_initialized = true;
        m_modified = false;
        syncFromPolicyGateway();
    }

    const QStringList issues = validateIntegrity();
    for (const QString& issue : issues) {
        qWarning() << "DatabaseManager: integrity:" << issue;
    }

    emit databaseLoaded(m_devices.size());
    return true;
}

QString DatabaseManager::policyActor() const
{
    return QStringLiteral("database");
}

void DatabaseManager::syncFromPolicyGateway()
{
    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    if (!gate) {
        return;
    }
    const Policy::PolicySnapshot snap = gate->snapshot();
    m_devices.clear();
    for (const DeviceRecord& rec : snap.devices) {
        m_devices.insert(rec.uniqueId, rec);
    }
}

bool DatabaseManager::persistDevice(const DeviceRecord& record, const QString& reason)
{
    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    if (!gate) {
        return false;
    }
    return gate->upsertDevice(record, policyActor(), reason);
}

bool DatabaseManager::persistRecordById(const QString& uniqueId, const QString& reason)
{
    QReadLocker locker(&m_lock);
    auto it = m_devices.find(uniqueId);
    if (it == m_devices.end()) {
        return false;
    }
    const DeviceRecord rec = *it;
    locker.unlock();
    return persistDevice(rec, reason);
}

bool DatabaseManager::isInitialized() const
{
    QReadLocker locker(&m_lock);
    return m_initialized;
}

QString DatabaseManager::databasePath() const
{
    QReadLocker locker(&m_lock);
    return m_databasePath;
}

bool DatabaseManager::hasDevice(const QString& uniqueId) const
{
    QReadLocker locker(&m_lock);
    return DeviceIdUtil::resolveStoredId(m_devices, uniqueId).has_value();
}

std::optional<DeviceRecord> DatabaseManager::getDevice(const QString& uniqueId) const
{
    QReadLocker locker(&m_lock);
    const std::optional<QString> storedId = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
    if (!storedId) {
        return std::nullopt;
    }
    return m_devices.value(*storedId);
}

QList<DeviceRecord> DatabaseManager::getAllDevices() const
{
    QReadLocker locker(&m_lock);
    return m_devices.values();
}

QList<DeviceRecord> DatabaseManager::getDevicesWhere(
    std::function<bool(const DeviceRecord&)> filter) const
{
    QReadLocker locker(&m_lock);
    QList<DeviceRecord> result;
    
    for (const auto& record : m_devices) {
        if (filter(record)) {
            result.append(record);
        }
    }
    
    return result;
}

bool DatabaseManager::addDevice(const DeviceRecord& record)
{
    {
        QWriteLocker locker(&m_lock);
        if (m_devices.contains(record.uniqueId)) {
            return false;
        }
    }
    if (!persistDevice(record, QStringLiteral("add"))) {
        return false;
    }
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceAdded(record.uniqueId);
    return true;
}

bool DatabaseManager::updateDevice(const DeviceRecord& record)
{
    {
        QWriteLocker locker(&m_lock);
        if (!m_devices.contains(record.uniqueId)) {
            return false;
        }
    }
    if (!persistDevice(record, QStringLiteral("update"))) {
        return false;
    }
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(record.uniqueId);
    return true;
}

void DatabaseManager::upsertDevice(const DeviceRecord& record)
{
    const bool isNew = [&] {
        QReadLocker locker(&m_lock);
        return !m_devices.contains(record.uniqueId);
    }();
    persistDevice(record, QStringLiteral("upsert"));
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    if (isNew) {
        emit deviceAdded(record.uniqueId);
    } else {
        emit deviceUpdated(record.uniqueId);
    }
}

bool DatabaseManager::removeDevice(const QString& uniqueId)
{
    {
        QWriteLocker locker(&m_lock);
        if (!m_devices.contains(uniqueId)) {
            return false;
        }
    }
    if (auto* gate = Policy::PolicyServiceLocator::gateway()) {
        gate->removeDevice(uniqueId, policyActor(), QStringLiteral("remove"));
    }
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceRemoved(uniqueId);
    return true;
}

int DatabaseManager::removeDevices(const QStringList& uniqueIds)
{
    int removed = 0;
    QStringList actuallyRemoved;

    {
        QReadLocker locker(&m_lock);
        for (const auto& id : uniqueIds) {
            if (m_devices.contains(id)) {
                actuallyRemoved.append(id);
            }
        }
    }

    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    for (const auto& id : actuallyRemoved) {
        if (gate && gate->removeDevice(id, policyActor(), QStringLiteral("bulk_remove"))) {
            ++removed;
        }
    }

    if (removed > 0) {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }

    for (const auto& id : actuallyRemoved) {
        emit deviceRemoved(id);
    }

    return removed;
}

void DatabaseManager::clearAllDevices()
{
    QStringList ids;
    {
        QReadLocker locker(&m_lock);
        ids = m_devices.keys();
    }
    if (auto* gate = Policy::PolicyServiceLocator::gateway()) {
        gate->clearDevices(policyActor(), QStringLiteral("clear_all"));
    }
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    for (const auto& id : ids) {
        emit deviceRemoved(id);
    }
}

bool DatabaseManager::updateHash(const QString& uniqueId, const QString& hash,
                                  const QString& algorithm, uint64_t durationMs,
                                  const QString& hashScope,
                                  const QString& hashScanMode)
{
    DeviceRecord rec;
    QString storedId;
    {
        QWriteLocker locker(&m_lock);

        const std::optional<QString> resolved = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
        if (!resolved) {
            return false;
        }
        storedId = *resolved;

        DeviceRecord& record = m_devices[storedId];
        record.hash = hash;
        record.hashAlgorithm = algorithm;
        record.hashDurationMs = durationMs;
        if (!hashScope.isEmpty()) {
            record.hashScope = hashScope;
        }
        if (!hashScanMode.isEmpty()) {
            record.hashScanMode = hashScanMode;
        }
        record.lastHashed = QDateTime::currentDateTime();
        rec = record;
    }
    if (!persistDevice(rec, QStringLiteral("update_hash"))) {
        return false;
    }
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(storedId);
    return true;
}

std::optional<QString> DatabaseManager::getHash(const QString& uniqueId) const
{
    QReadLocker locker(&m_lock);

    const std::optional<QString> storedId = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
    if (!storedId) {
        return std::nullopt;
    }

    const DeviceRecord& rec = m_devices.value(*storedId);
    if (!rec.hash.isEmpty()) {
        return rec.hash;
    }

    return std::nullopt;
}

bool DatabaseManager::verifyHash(const QString& uniqueId, const QString& hash) const
{
    QReadLocker locker(&m_lock);

    const std::optional<QString> storedId = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
    if (!storedId) {
        return false;
    }

    const DeviceRecord& rec = m_devices.value(*storedId);
    const bool matches = (rec.hash.compare(hash, Qt::CaseInsensitive) == 0);

    if (!matches && !rec.hash.isEmpty()) {
        const QString id = *storedId;
        const QString expected = rec.hash;
        locker.unlock();
        const_cast<DatabaseManager*>(this)->reportHashMismatch(id, expected, hash);
    }

    return matches;
}

bool DatabaseManager::verifyHash(const DeviceInfo& device, const QString& hash) const
{
    QReadLocker locker(&m_lock);

    std::optional<QString> storedId = DeviceIdUtil::resolveStoredId(m_devices, device.uniqueId());
    if (!storedId) {
        storedId = DeviceIdUtil::resolveStoredId(m_devices, device.partitionUniqueId());
    }
    if (!storedId) {
        return false;
    }

    const DeviceRecord& rec = m_devices.value(*storedId);
    const bool matches = (rec.hash.compare(hash, Qt::CaseInsensitive) == 0);

    if (!matches && !rec.hash.isEmpty()) {
        const QString id = *storedId;
        const QString expected = rec.hash;
        locker.unlock();
        const_cast<DatabaseManager*>(this)->reportHashMismatch(id, expected, hash);
    }

    return matches;
}

void DatabaseManager::reportHashMismatch(const QString& uniqueId, const QString& expected,
                                         const QString& actual)
{
    emit hashMismatch(uniqueId, expected, actual);
}

bool DatabaseManager::setTrustLevel(const QString& uniqueId, int level)
{
    {
        QWriteLocker locker(&m_lock);
        
        auto it = m_devices.find(uniqueId);
        if (it == m_devices.end()) {
            return false;
        }
        
        it->trustLevel = level;
    }
    persistRecordById(uniqueId, QStringLiteral("set_trust"));
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(uniqueId);
    return true;
}

bool DatabaseManager::setAutoMount(const QString& uniqueId, bool autoMount)
{
    {
        QWriteLocker locker(&m_lock);
        
        auto it = m_devices.find(uniqueId);
        if (it == m_devices.end()) {
            return false;
        }
        
        it->autoMount = autoMount;
    }
    persistRecordById(uniqueId, QStringLiteral("set_auto_mount"));
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(uniqueId);
    return true;
}

bool DatabaseManager::updateLastSeen(const QString& uniqueId)
{
    DeviceRecord rec;
    {
        QWriteLocker locker(&m_lock);
        const std::optional<QString> storedId = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
        if (!storedId) {
            return false;
        }
        m_devices[*storedId].lastSeen = QDateTime::currentDateTime();
        rec = m_devices.value(*storedId);
    }

    if (!persistDevice(rec, QStringLiteral("last_seen"))) {
        return false;
    }

    QWriteLocker locker(&m_lock);
    syncFromPolicyGateway();
    m_modified = false;
    return true;
}

bool DatabaseManager::save()
{
    QWriteLocker locker(&m_lock);
    if (!m_initialized) {
        emit databaseError(QStringLiteral("Database not initialized"));
        return false;
    }
    m_modified = false;
    m_lastSaved = QDateTime::currentDateTime();
    locker.unlock();
    emit databaseSaved();
    return true;
}

bool DatabaseManager::reload()
{
    if (auto* gate = Policy::PolicyServiceLocator::gateway()) {
        QString err;
        if (!gate->reload(&err)) {
            emit databaseError(err);
            return false;
        }
    }
    QWriteLocker locker(&m_lock);
    if (!m_initialized) {
        return false;
    }
    syncFromPolicyGateway();
    m_modified = false;
    locker.unlock();
    emit databaseLoaded(m_devices.size());
    return true;
}

QString DatabaseManager::createBackup(const QString& backupPath)
{
    QReadLocker locker(&m_lock);
    
    if (!m_initialized) {
        return QString();
    }
    
    QString destPath = backupPath;
    if (destPath.isEmpty()) {
        QFileInfo fi(m_databasePath);
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        destPath = fi.absolutePath() + "/" + fi.baseName() + 
                   "_backup_" + timestamp + "." + fi.suffix();
    }
    
    const QString storePath = Policy::PolicyPaths::storeFilePath();
    if (QFile::exists(storePath)) {
        if (QFile::copy(storePath, destPath)) {
            m_lastBackup = QDateTime::currentDateTime();
            
            // Cleanup old backups
            QDir backupDir(QFileInfo(destPath).absolutePath());
            QStringList filters;
            filters << QFileInfo(m_databasePath).baseName() + "_backup_*";
            QFileInfoList backups = backupDir.entryInfoList(filters, QDir::Files, QDir::Time);
            
            while (backups.size() > MAX_BACKUP_COUNT) {
                QFile::remove(backups.takeLast().absoluteFilePath());
            }
            
            return destPath;
        }
    }
    
    return QString();
}

bool DatabaseManager::restoreFromBackup(const QString& backupPath)
{
    if (!QFile::exists(backupPath)) {
        emit databaseError("Backup file not found: " + backupPath);
        return false;
    }

    createBackup();

    const QString storePath = Policy::PolicyPaths::storeFilePath();
    if (QFile::exists(storePath)) {
        QFile::remove(storePath);
    }
    if (!QFile::copy(backupPath, storePath)) {
        emit databaseError(QStringLiteral("Failed to install backup into policy store"));
        return false;
    }

    if (auto* gate = Policy::PolicyServiceLocator::gateway()) {
        QString err;
        if (!gate->reload(&err)) {
            emit databaseError(err.isEmpty() ? QStringLiteral("Policy reload failed") : err);
            return false;
        }
    }

    QWriteLocker locker(&m_lock);
    syncFromPolicyGateway();
    m_modified = false;
    locker.unlock();
    emit databaseLoaded(m_devices.size());
    return true;
}

bool DatabaseManager::exportToFile(const QString& path, bool prettyPrint) const
{
    if (auto* gate = Policy::PolicyServiceLocator::gateway()) {
        return gate->exportJson(path, prettyPrint);
    }
    return false;
}

int DatabaseManager::importFromFile(const QString& path, bool merge)
{
    if (auto* gate = Policy::PolicyServiceLocator::gateway()) {
        const int count = gate->importJson(path, merge, policyActor());
        if (count >= 0) {
            QWriteLocker locker(&m_lock);
            syncFromPolicyGateway();
            m_modified = false;
        }
        return count;
    }
    return -1;
}

DatabaseManager::Stats DatabaseManager::getStats() const
{
    QReadLocker locker(&m_lock);
    
    Stats stats;
    stats.totalDevices = m_devices.size();
    stats.lastModified = m_lastSaved;
    stats.lastBackup = m_lastBackup;
    
    for (const auto& record : m_devices) {
        if (record.trustLevel > 0) {
            stats.trustedDevices++;
        }
        if (record.autoMount) {
            stats.autoMountDevices++;
        }
    }
    
    QFileInfo fi(m_databasePath);
    if (fi.exists()) {
        stats.fileSizeBytes = fi.size();
    }
    
    return stats;
}

int DatabaseManager::deviceCount() const
{
    QReadLocker locker(&m_lock);
    return m_devices.size();
}

bool DatabaseManager::hasUnsavedChanges() const
{
    QReadLocker locker(&m_lock);
    return m_modified;
}

void DatabaseManager::setAutoSave(bool enabled)
{
    QWriteLocker locker(&m_lock);
    m_autoSave = enabled;
}

QStringList DatabaseManager::validateIntegrity() const
{
    QReadLocker locker(&m_lock);
    QStringList issues;
    
    for (const auto& record : m_devices) {
        if (record.uniqueId.isEmpty()) {
            issues.append("Found device with empty unique ID");
        }
        if (record.trustLevel > 0 && record.uniqueId.isEmpty() == false) {
            const bool needsHash = record.verificationProfile == VerificationProfile::FullPartition
                                   || record.verificationProfile == VerificationProfile::Hybrid;
            if (needsHash && record.hash.isEmpty()) {
                issues.append(QStringLiteral("Trusted device %1 uses full-partition verification but has no hash")
                                  .arg(record.uniqueId));
            }
        }
        if (!record.firstSeen.isValid()) {
            issues.append(QString("Device %1 has invalid first_seen date").arg(record.uniqueId));
        }
    }
    
    return issues;
}

void DatabaseManager::compact()
{
    QStringList toRemove;
    {
        QReadLocker locker(&m_lock);
        for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
            if (it->uniqueId.isEmpty()) {
                toRemove.append(it.key());
            }
        }
    }

    if (toRemove.isEmpty()) {
        return;
    }

    Policy::PolicyGateway* gate = Policy::PolicyServiceLocator::gateway();
    for (const auto& id : toRemove) {
        if (gate) {
            gate->removeDevice(id, policyActor(), QStringLiteral("compact"));
        }
    }

    QWriteLocker locker(&m_lock);
    syncFromPolicyGateway();
    m_modified = false;
}

QString DatabaseManager::defaultDatabasePath()
{
    return Policy::PolicyPaths::storeFilePath();
}

void DatabaseManager::markModified()
{
    m_modified = true;
    
    if (m_autoSave) {
        // Don't save while holding lock - schedule it
        QMetaObject::invokeMethod(this, [this]() {
            save();
        }, Qt::QueuedConnection);
    }
}


bool DatabaseManager::hasDevice(const DeviceInfo& device) const
{
    return hasDevice(canonicalUniqueId(device)) || hasDevice(device.uniqueId());
}

QString DatabaseManager::canonicalUniqueId(const DeviceInfo& device) const
{
    return device.partitionUniqueId();
}

std::optional<DeviceRecord> DatabaseManager::getDevice(const DeviceInfo& device) const
{
    if (auto rec = getDevice(device.uniqueId())) {
        return rec;
    }
    return getDevice(canonicalUniqueId(device));
}



bool DatabaseManager::updateIsoBaselines(const QString& uniqueId,
                                         const QList<IsoImageBaseline>& baselines)
{
    DeviceRecord rec;
    QString storedId;
    {
        QWriteLocker locker(&m_lock);
        const std::optional<QString> resolved = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
        if (!resolved) {
            return false;
        }
        storedId = *resolved;
        m_devices[storedId].isoBaselines = baselines;
        rec = m_devices.value(storedId);
    }

    if (!persistDevice(rec, QStringLiteral("iso_baselines"))) {
        return false;
    }

    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(storedId);
    return true;
}

bool DatabaseManager::updateWatchManifest(const QString& uniqueId, const WatchManifest& manifest)
{
    DeviceRecord rec;
    QString storedId;
    {
        QWriteLocker locker(&m_lock);
        const std::optional<QString> resolved = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
        if (!resolved) {
            return false;
        }
        storedId = *resolved;
        m_devices[storedId].watchManifest = manifest;
        m_devices[storedId].lastManifestRoot = manifest.manifestRoot;
        rec = m_devices.value(storedId);
    }

    if (!persistDevice(rec, QStringLiteral("watch_manifest"))) {
        return false;
    }

    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(storedId);
    return true;
}

bool DatabaseManager::setVerificationProfile(const QString& uniqueId, VerificationProfile profile)
{
    DeviceRecord rec;
    QString storedId;
    {
        QWriteLocker locker(&m_lock);
        const std::optional<QString> resolved = DeviceIdUtil::resolveStoredId(m_devices, uniqueId);
        if (!resolved) {
            return false;
        }
        storedId = *resolved;
        m_devices[storedId].verificationProfile = profile;
        rec = m_devices.value(storedId);
    }

    if (!persistDevice(rec, QStringLiteral("verification_profile"))) {
        return false;
    }

    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(storedId);
    return true;
}

} // namespace FlashSpartan
