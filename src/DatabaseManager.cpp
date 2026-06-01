#include "DatabaseManager.h"
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
    Q_UNUSED(path);
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
    if (m_devices.contains(uniqueId)) {
        return true;
    }

    // Partition-aware id (SERIAL_Vendor_Model_sdb1) may match a legacy record
    // stored without the partition suffix (SERIAL_Vendor_Model).
    const int sep = uniqueId.lastIndexOf(QLatin1Char('_'));
    if (sep > 0) {
        const QString tail = uniqueId.mid(sep + 1);
        if (tail.startsWith(QLatin1String("sd"))
            || tail.startsWith(QLatin1String("mmcblk"))
            || tail.startsWith(QLatin1String("nvme"))) {
            return m_devices.contains(uniqueId.left(sep));
        }
    }

    return false;
}

std::optional<DeviceRecord> DatabaseManager::getDevice(const QString& uniqueId) const
{
    QReadLocker locker(&m_lock);
    auto it = m_devices.find(uniqueId);
    if (it != m_devices.end()) {
        return *it;
    }
    return std::nullopt;
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
        if (gate) {
            gate->removeDevice(id, policyActor(), QStringLiteral("bulk_remove"));
        }
        ++removed;
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
    {
        QWriteLocker locker(&m_lock);
        
        auto it = m_devices.find(uniqueId);
        if (it == m_devices.end()) {
            return false;
        }
        
        it->hash = hash;
        it->hashAlgorithm = algorithm;
        it->hashDurationMs = durationMs;
        if (!hashScope.isEmpty()) {
            it->hashScope = hashScope;
        }
        if (!hashScanMode.isEmpty()) {
            it->hashScanMode = hashScanMode;
        }
        it->lastHashed = QDateTime::currentDateTime();
    }
    persistRecordById(uniqueId, QStringLiteral("update_hash"));
    {
        QWriteLocker locker(&m_lock);
        syncFromPolicyGateway();
        m_modified = false;
    }
    emit deviceUpdated(uniqueId);
    return true;
}

std::optional<QString> DatabaseManager::getHash(const QString& uniqueId) const
{
    QReadLocker locker(&m_lock);
    
    auto it = m_devices.find(uniqueId);
    if (it != m_devices.end() && !it->hash.isEmpty()) {
        return it->hash;
    }
    
    return std::nullopt;
}

bool DatabaseManager::verifyHash(const QString& uniqueId, const QString& hash) const
{
    QReadLocker locker(&m_lock);
    
    auto it = m_devices.find(uniqueId);
    if (it == m_devices.end()) {
        return false;
    }
    
    bool matches = (it->hash.compare(hash, Qt::CaseInsensitive) == 0);
    
    if (!matches && !it->hash.isEmpty()) {
        // Use const_cast to emit from const method, or use mutable
        const_cast<DatabaseManager*>(this)->emit hashMismatch(
            uniqueId, it->hash, hash);
    }
    
    return matches;
}

bool DatabaseManager::verifyHash(const DeviceInfo& device, const QString& hash) const
{
    QReadLocker locker(&m_lock);
    auto it = m_devices.find(device.uniqueId());
    if (it == m_devices.end()) {
        it = m_devices.find(canonicalUniqueId(device));
    }
    if (it == m_devices.end()) {
        return false;
    }
    return verifyHash(it->uniqueId, hash);
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
    QWriteLocker locker(&m_lock);
    
    auto it = m_devices.find(uniqueId);
    if (it == m_devices.end()) {
        return false;
    }
    
    it->lastSeen = QDateTime::currentDateTime();
    const bool ok = persistRecordById(uniqueId, QStringLiteral("last_seen"));
    syncFromPolicyGateway();
    m_modified = false;
    return ok;
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
        if (record.hash.isEmpty() && record.trustLevel > 0) {
            issues.append(QString("Trusted device %1 has no hash").arg(record.uniqueId));
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

bool DatabaseManager::loadFromFile()
{
    QFile file(m_databasePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "DatabaseManager: Failed to open" << m_databasePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    if (data.isEmpty()) {
        // Empty file is valid
        return true;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "DatabaseManager: JSON parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Version check
    QString version = root["version"].toString();
    if (!version.isEmpty() && version != DB_VERSION) {
        qInfo() << "DatabaseManager: Migrating from version" << version << "to" << DB_VERSION;
        // Future: add migration logic here
    }
    
    // Load devices
    QJsonArray devicesArray = root["devices"].toArray();
    
    for (const auto& item : devicesArray) {
        DeviceRecord record = DeviceRecord::fromJson(item.toObject());
        if (!record.uniqueId.isEmpty()) {
            m_devices.insert(record.uniqueId, record);
        }
    }
    
    qInfo() << "DatabaseManager: Loaded" << m_devices.size() << "devices from" << m_databasePath;
    return true;
}

bool DatabaseManager::writeToFile()
{
    // Build JSON document
    QJsonObject root;
    root["version"] = DB_VERSION;
    root["last_modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["device_count"] = static_cast<int>(m_devices.size());
    
    QJsonArray devicesArray;
    for (const auto& record : m_devices) {
        devicesArray.append(record.toJson());
    }
    root["devices"] = devicesArray;
    
    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    // Write to temporary file first for atomic update
    QString tempPath = m_databasePath + ".tmp";
    QFile tempFile(tempPath);
    
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "DatabaseManager: Failed to open temp file for writing";
        emit databaseError("Failed to write database");
        return false;
    }
    
    qint64 written = tempFile.write(data);
    tempFile.flush();
    tempFile.close();
    
    if (written != data.size()) {
        QFile::remove(tempPath);
        emit databaseError("Failed to write complete database");
        return false;
    }
    
    // Atomic rename
    if (QFile::exists(m_databasePath)) {
        QFile::remove(m_databasePath);
    }
    
    if (!QFile::rename(tempPath, m_databasePath)) {
        emit databaseError("Failed to finalize database write");
        return false;
    }
    
    // Set secure permissions (owner read/write only)
    QFile::setPermissions(m_databasePath, 
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    
    qInfo() << "DatabaseManager: Saved" << m_devices.size() << "devices to" << m_databasePath;
    return true;
}

bool DatabaseManager::ensureDirectory()
{
    QFileInfo fi(m_databasePath);
    QDir dir = fi.absoluteDir();
    
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "DatabaseManager: Failed to create directory" << dir.path();
            return false;
        }
    }
    
    return true;
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
    return getDevice(canonicalUniqueId(device));
}



bool DatabaseManager::updateWatchManifest(const QString& uniqueId, const WatchManifest& manifest)
{
    QWriteLocker locker(&m_lock);
    auto it = m_devices.find(uniqueId);
    if (it == m_devices.end()) return false;
    it->watchManifest = manifest;
    it->lastManifestRoot = manifest.manifestRoot;
    persistRecordById(uniqueId, QStringLiteral("watch_manifest"));
    syncFromPolicyGateway();
    m_modified = false;
    emit deviceUpdated(uniqueId);
    return true;
}

bool DatabaseManager::setVerificationProfile(const QString& uniqueId, VerificationProfile profile)
{
    QWriteLocker locker(&m_lock);
    auto it = m_devices.find(uniqueId);
    if (it == m_devices.end()) {
        return false;
    }
    it->verificationProfile = profile;
    persistRecordById(uniqueId, QStringLiteral("verification_profile"));
    syncFromPolicyGateway();
    m_modified = false;
    emit deviceUpdated(uniqueId);
    return true;
}

} // namespace FlashSpartan
