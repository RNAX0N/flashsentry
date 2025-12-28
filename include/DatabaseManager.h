#pragma once

#include <QObject>
#include <QMutex>
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QDateTime>
#include <memory>
#include <optional>

#include "Types.h"

namespace FlashSentry {

/**
 * @brief DatabaseManager - Thread-safe persistent storage for device records
 * 
 * Manages the whitelist database with atomic read/write operations.
 * Uses JSON for human-readable storage with automatic backups.
 * All public methods are thread-safe.
 */
class DatabaseManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Database statistics
     */
    struct Stats {
        int totalDevices = 0;
        int trustedDevices = 0;
        int autoMountDevices = 0;
        QDateTime lastModified;
        QDateTime lastBackup;
        qint64 fileSizeBytes = 0;
    };

    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    // Prevent copying
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    /**
     * @brief Initialize the database
     * @param path Path to the database file (JSON)
     * @return true if successful
     */
    bool initialize(const QString& path);

    /**
     * @brief Check if database is initialized and valid
     */
    bool isInitialized() const;

    /**
     * @brief Get the database file path
     */
    QString databasePath() const;

    // ========================================================================
    // Device Record Operations
    // ========================================================================

    /**
     * @brief Check if a device is in the whitelist
     * @param uniqueId Device unique identifier
     */
    bool hasDevice(const QString& uniqueId) const;

    /**
     * @brief Get a device record
     * @param uniqueId Device unique identifier
     * @return Device record if found
     */
    std::optional<DeviceRecord> getDevice(const QString& uniqueId) const;

    /**
     * @brief Get all device records
     */
    QList<DeviceRecord> getAllDevices() const;

    /**
     * @brief Get devices matching a filter
     * @param filter Lambda that returns true for matching devices
     */
    QList<DeviceRecord> getDevicesWhere(
        std::function<bool(const DeviceRecord&)> filter) const;

    /**
     * @brief Add a new device to the whitelist
     * @param record Device record to add
     * @return true if added (false if already exists)
     */
    bool addDevice(const DeviceRecord& record);

    /**
     * @brief Update an existing device record
     * @param record Updated device record
     * @return true if updated (false if not found)
     */
    bool updateDevice(const DeviceRecord& record);

    /**
     * @brief Add or update a device record
     * @param record Device record
     */
    void upsertDevice(const DeviceRecord& record);

    /**
     * @brief Remove a device from the whitelist
     * @param uniqueId Device unique identifier
     * @return true if removed
     */
    bool removeDevice(const QString& uniqueId);

    /**
     * @brief Remove multiple devices
     * @param uniqueIds List of device identifiers
     * @return Number of devices removed
     */
    int removeDevices(const QStringList& uniqueIds);

    /**
     * @brief Clear all devices from the database
     */
    void clearAllDevices();

    // ========================================================================
    // Hash Operations
    // ========================================================================

    /**
     * @brief Update the hash for a device
     * @param uniqueId Device identifier
     * @param hash New hash value
     * @param algorithm Hash algorithm used
     * @param durationMs Time taken to compute hash
     * @return true if updated
     */
    bool updateHash(const QString& uniqueId, const QString& hash,
                    const QString& algorithm = "SHA256",
                    uint64_t durationMs = 0);

    /**
     * @brief Get the stored hash for a device
     * @param uniqueId Device identifier
     * @return Hash string if found
     */
    std::optional<QString> getHash(const QString& uniqueId) const;

    /**
     * @brief Check if a hash matches the stored hash
     * @param uniqueId Device identifier
     * @param hash Hash to compare
     * @return true if matches, false if different or not found
     */
    bool verifyHash(const QString& uniqueId, const QString& hash) const;

    // ========================================================================
    // Trust Level Operations
    // ========================================================================

    /**
     * @brief Set the trust level for a device
     * @param uniqueId Device identifier
     * @param level Trust level (0=new, 1=trusted, 2=always allow)
     */
    bool setTrustLevel(const QString& uniqueId, int level);

    /**
     * @brief Set auto-mount flag for a device
     * @param uniqueId Device identifier
     * @param autoMount Whether to auto-mount this device
     */
    bool setAutoMount(const QString& uniqueId, bool autoMount);

    /**
     * @brief Update last seen timestamp for a device
     * @param uniqueId Device identifier
     */
    bool updateLastSeen(const QString& uniqueId);

    // ========================================================================
    // Persistence Operations
    // ========================================================================

    /**
     * @brief Save the database to disk
     * @return true if successful
     */
    bool save();

    /**
     * @brief Reload the database from disk
     * @return true if successful
     */
    bool reload();

    /**
     * @brief Create a backup of the database
     * @param backupPath Optional custom backup path
     * @return Path to backup file, or empty string on failure
     */
    QString createBackup(const QString& backupPath = QString());

    /**
     * @brief Restore from a backup
     * @param backupPath Path to backup file
     * @return true if successful
     */
    bool restoreFromBackup(const QString& backupPath);

    /**
     * @brief Export database to a file
     * @param path Export file path
     * @param prettyPrint Whether to format JSON for readability
     * @return true if successful
     */
    bool exportToFile(const QString& path, bool prettyPrint = true) const;

    /**
     * @brief Import devices from a file
     * @param path Import file path
     * @param merge If true, merge with existing; if false, replace all
     * @return Number of devices imported (-1 on error)
     */
    int importFromFile(const QString& path, bool merge = true);

    // ========================================================================
    // Statistics and Maintenance
    // ========================================================================

    /**
     * @brief Get database statistics
     */
    Stats getStats() const;

    /**
     * @brief Get number of devices in database
     */
    int deviceCount() const;

    /**
     * @brief Check if there are unsaved changes
     */
    bool hasUnsavedChanges() const;

    /**
     * @brief Set whether to auto-save on changes
     */
    void setAutoSave(bool enabled);

    /**
     * @brief Validate database integrity
     * @return List of issues found (empty if valid)
     */
    QStringList validateIntegrity() const;

    /**
     * @brief Compact the database (remove redundant data)
     */
    void compact();

signals:
    /**
     * @brief Emitted when a device is added
     */
    void deviceAdded(const QString& uniqueId);

    /**
     * @brief Emitted when a device is updated
     */
    void deviceUpdated(const QString& uniqueId);

    /**
     * @brief Emitted when a device is removed
     */
    void deviceRemoved(const QString& uniqueId);

    /**
     * @brief Emitted when the database is saved
     */
    void databaseSaved();

    /**
     * @brief Emitted when the database is loaded/reloaded
     */
    void databaseLoaded(int deviceCount);

    /**
     * @brief Emitted on database errors
     */
    void databaseError(const QString& error);

    /**
     * @brief Emitted when hash verification fails
     */
    void hashMismatch(const QString& uniqueId, const QString& expected, 
                      const QString& actual);

private:
    /**
     * @brief Load database from file
     */
    bool loadFromFile();

    /**
     * @brief Write database to file
     */
    bool writeToFile();

    /**
     * @brief Ensure the database directory exists
     */
    bool ensureDirectory();

    /**
     * @brief Get default database path
     */
    static QString defaultDatabasePath();

    /**
     * @brief Mark database as modified
     */
    void markModified();

    // Database file path
    QString m_databasePath;

    // Device records (uniqueId -> record)
    QHash<QString, DeviceRecord> m_devices;

    // Thread safety
    mutable QReadWriteLock m_lock;

    // State
    bool m_initialized = false;
    bool m_modified = false;
    bool m_autoSave = true;
    QDateTime m_lastSaved;
    QDateTime m_lastBackup;

    // Configuration
    static constexpr int MAX_BACKUP_COUNT = 5;
    static constexpr const char* DB_VERSION = "1.0";
};

} // namespace FlashSentry