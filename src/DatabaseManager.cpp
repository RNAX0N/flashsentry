#include "DatabaseManager.h"

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

namespace FlashSentry {

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
    QWriteLocker locker(&m_lock);
    
    m_databasePath = path.isEmpty() ? defaultDatabasePath() : path;
    
    if (!ensureDirectory()) {
        emit databaseError("Failed to create database directory");
        return false;
    }
    
    // Try to load existing database
    if (QFile::exists(m_databasePath)) {
        if (!loadFromFile()) {
            emit databaseError("Failed to load database, creating new one");
            m_devices.clear();
        }
    }
    
    m_initialized = true;
    m_modified = false;
    
    emit databaseLoaded(m_devices.size());
    return true;
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
    return m_devices.contains(uniqueId);
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
        
        m_devices.insert(record.uniqueId, record);
        markModified();
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
        
        m_devices[record.uniqueId] = record;
        markModified();
    }
    
    emit deviceUpdated(record.uniqueId);
    return true;
}

void DatabaseManager::upsertDevice(const DeviceRecord& record)
{
    bool isNew;
    
    {
        QWriteLocker locker(&m_lock);
        isNew = !m_devices.contains(record.uniqueId);
        m_devices[record.uniqueId] = record;
        markModified();
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
        
        if (!m_devices.remove(uniqueId)) {
            return false;
        }
        
        markModified();
    }
    
    emit deviceRemoved(uniqueId);
    return true;
}

int DatabaseManager::removeDevices(const QStringList& uniqueIds)
{
    int removed = 0;
    QStringList actuallyRemoved;
    
    {
        QWriteLocker locker(&m_lock);
        
        for (const auto& id : uniqueIds) {
            if (m_devices.remove(id)) {
                removed++;
                actuallyRemoved.append(id);
            }
        }
        
        if (removed > 0) {
            markModified();
        }
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
        QWriteLocker locker(&m_lock);
        ids = m_devices.keys();
        m_devices.clear();
        markModified();
    }
    
    for (const auto& id : ids) {
        emit deviceRemoved(id);
    }
}

bool DatabaseManager::updateHash(const QString& uniqueId, const QString& hash,
                                  const QString& algorithm, uint64_t durationMs)
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
        it->lastHashed = QDateTime::currentDateTime();
        
        markModified();
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

bool DatabaseManager::setTrustLevel(const QString& uniqueId, int level)
{
    {
        QWriteLocker locker(&m_lock);
        
        auto it = m_devices.find(uniqueId);
        if (it == m_devices.end()) {
            return false;
        }
        
        it->trustLevel = level;
        markModified();
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
        markModified();
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
    markModified();
    
    return true;
}

bool DatabaseManager::save()
{
    QWriteLocker locker(&m_lock);
    
    if (!m_initialized) {
        emit databaseError("Database not initialized");
        return false;
    }
    
    if (writeToFile()) {
        m_modified = false;
        m_lastSaved = QDateTime::currentDateTime();
        locker.unlock();
        emit databaseSaved();
        return true;
    }
    
    return false;
}

bool DatabaseManager::reload()
{
    QWriteLocker locker(&m_lock);
    
    if (!m_initialized) {
        return false;
    }
    
    m_devices.clear();
    
    if (loadFromFile()) {
        m_modified = false;
        locker.unlock();
        emit databaseLoaded(m_devices.size());
        return true;
    }
    
    return false;
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
    
    // Copy current database file
    if (QFile::exists(m_databasePath)) {
        if (QFile::copy(m_databasePath, destPath)) {
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
    
    // Create a backup of current state first
    createBackup();
    
    QWriteLocker locker(&m_lock);
    
    // Read backup file
    QFile file(backupPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit databaseError("Failed to open backup file");
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit databaseError("Invalid backup file: " + error.errorString());
        return false;
    }
    
    // Parse and load
    QJsonObject root = doc.object();
    QJsonArray devicesArray = root["devices"].toArray();
    
    m_devices.clear();
    
    for (const auto& item : devicesArray) {
        DeviceRecord record = DeviceRecord::fromJson(item.toObject());
        m_devices.insert(record.uniqueId, record);
    }
    
    m_modified = true;
    
    if (!writeToFile()) {
        emit databaseError("Failed to save restored database");
        return false;
    }
    
    m_modified = false;
    locker.unlock();
    
    emit databaseLoaded(m_devices.size());
    return true;
}

bool DatabaseManager::exportToFile(const QString& path, bool prettyPrint) const
{
    QReadLocker locker(&m_lock);
    
    QJsonObject root;
    root["version"] = DB_VERSION;
    root["exported"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["device_count"] = static_cast<int>(m_devices.size());
    
    QJsonArray devicesArray;
    for (const auto& record : m_devices) {
        devicesArray.append(record.toJson());
    }
    root["devices"] = devicesArray;
    
    QJsonDocument doc(root);
    QByteArray data = prettyPrint ? doc.toJson(QJsonDocument::Indented) 
                                  : doc.toJson(QJsonDocument::Compact);
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    
    qint64 written = file.write(data);
    file.close();
    
    return written == data.size();
}

int DatabaseManager::importFromFile(const QString& path, bool merge)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit databaseError("Failed to open import file: " + path);
        return -1;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit databaseError("Invalid import file: " + error.errorString());
        return -1;
    }
    
    QJsonObject root = doc.object();
    QJsonArray devicesArray = root["devices"].toArray();
    
    int imported = 0;
    
    {
        QWriteLocker locker(&m_lock);
        
        if (!merge) {
            m_devices.clear();
        }
        
        for (const auto& item : devicesArray) {
            DeviceRecord record = DeviceRecord::fromJson(item.toObject());
            
            if (!m_devices.contains(record.uniqueId)) {
                m_devices.insert(record.uniqueId, record);
                imported++;
            } else if (!merge) {
                m_devices[record.uniqueId] = record;
                imported++;
            }
        }
        
        if (imported > 0) {
            markModified();
        }
    }
    
    return imported;
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
    QWriteLocker locker(&m_lock);
    
    // Remove devices with invalid data
    QStringList toRemove;
    
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        if (it->uniqueId.isEmpty()) {
            toRemove.append(it.key());
        }
    }
    
    for (const auto& id : toRemove) {
        m_devices.remove(id);
    }
    
    if (!toRemove.isEmpty()) {
        markModified();
    }
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
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + "/flashsentry/devices.json";
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

} // namespace FlashSentry