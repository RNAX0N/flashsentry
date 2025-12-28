#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <cstdint>

namespace FlashSentry {

// ============================================================================
// Device Information
// ============================================================================

struct DeviceInfo {
    QString deviceNode;      // e.g., /dev/sdb1
    QString parentDevice;    // e.g., /dev/sdb
    QString serial;          // Device serial number
    QString vendor;          // Manufacturer
    QString model;           // Device model name
    QString label;           // Filesystem label
    QString fsType;          // Filesystem type (ext4, ntfs, etc.)
    QString mountPoint;      // Current mount point (empty if not mounted)
    uint64_t sizeBytes = 0;  // Partition size in bytes
    bool isRemovable = true;
    bool isMounted = false;
    
    QString displayName() const {
        if (!label.isEmpty()) return label;
        if (!model.isEmpty()) return model;
        return deviceNode.split('/').last();
    }
    
    QString uniqueId() const {
        return serial.isEmpty() ? 
            QString("%1_%2").arg(vendor, model) :
            QString("%1_%2_%3").arg(serial, vendor, model);
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["device_node"] = deviceNode;
        obj["serial"] = serial;
        obj["vendor"] = vendor;
        obj["model"] = model;
        obj["label"] = label;
        obj["fs_type"] = fsType;
        obj["size_bytes"] = static_cast<qint64>(sizeBytes);
        return obj;
    }
    
    static DeviceInfo fromJson(const QJsonObject& obj) {
        DeviceInfo info;
        info.deviceNode = obj["device_node"].toString();
        info.serial = obj["serial"].toString();
        info.vendor = obj["vendor"].toString();
        info.model = obj["model"].toString();
        info.label = obj["label"].toString();
        info.fsType = obj["fs_type"].toString();
        info.sizeBytes = static_cast<uint64_t>(obj["size_bytes"].toInteger());
        return info;
    }
};

// ============================================================================
// Device Record (stored in database)
// ============================================================================

struct DeviceRecord {
    QString uniqueId;
    QString hash;
    QString hashAlgorithm = "SHA256";
    QDateTime firstSeen;
    QDateTime lastSeen;
    QDateTime lastHashed;
    uint64_t hashDurationMs = 0;
    int trustLevel = 0;  // 0 = new, 1 = trusted, 2 = always allow
    bool autoMount = false;
    QString notes;
    DeviceInfo lastKnownInfo;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["unique_id"] = uniqueId;
        obj["hash"] = hash;
        obj["hash_algorithm"] = hashAlgorithm;
        obj["first_seen"] = firstSeen.toString(Qt::ISODate);
        obj["last_seen"] = lastSeen.toString(Qt::ISODate);
        obj["last_hashed"] = lastHashed.toString(Qt::ISODate);
        obj["hash_duration_ms"] = static_cast<qint64>(hashDurationMs);
        obj["trust_level"] = trustLevel;
        obj["auto_mount"] = autoMount;
        obj["notes"] = notes;
        obj["device_info"] = lastKnownInfo.toJson();
        return obj;
    }
    
    static DeviceRecord fromJson(const QJsonObject& obj) {
        DeviceRecord record;
        record.uniqueId = obj["unique_id"].toString();
        record.hash = obj["hash"].toString();
        record.hashAlgorithm = obj["hash_algorithm"].toString("SHA256");
        record.firstSeen = QDateTime::fromString(obj["first_seen"].toString(), Qt::ISODate);
        record.lastSeen = QDateTime::fromString(obj["last_seen"].toString(), Qt::ISODate);
        record.lastHashed = QDateTime::fromString(obj["last_hashed"].toString(), Qt::ISODate);
        record.hashDurationMs = static_cast<uint64_t>(obj["hash_duration_ms"].toInteger());
        record.trustLevel = obj["trust_level"].toInt();
        record.autoMount = obj["auto_mount"].toBool();
        record.notes = obj["notes"].toString();
        record.lastKnownInfo = DeviceInfo::fromJson(obj["device_info"].toObject());
        return record;
    }
};

// ============================================================================
// Hash Result
// ============================================================================

struct HashResult {
    QString deviceNode;
    QString hash;
    QString algorithm;
    uint64_t bytesProcessed = 0;
    uint64_t durationMs = 0;
    bool success = false;
    QString errorMessage;
    
    double speedMBps() const {
        if (durationMs == 0) return 0.0;
        return (static_cast<double>(bytesProcessed) / (1024.0 * 1024.0)) / 
               (static_cast<double>(durationMs) / 1000.0);
    }
};

// ============================================================================
// Verification Status
// ============================================================================

enum class VerificationStatus {
    Unknown,
    Pending,
    Hashing,
    Verified,      // Hash matches
    Modified,      // Hash mismatch
    NewDevice,     // Not in database
    Error          // Hashing failed
};

inline QString verificationStatusToString(VerificationStatus status) {
    switch (status) {
        case VerificationStatus::Unknown:   return "Unknown";
        case VerificationStatus::Pending:   return "Pending";
        case VerificationStatus::Hashing:   return "Hashing...";
        case VerificationStatus::Verified:  return "Verified";
        case VerificationStatus::Modified:  return "MODIFIED";
        case VerificationStatus::NewDevice: return "New Device";
        case VerificationStatus::Error:     return "Error";
    }
    return "Unknown";
}

// ============================================================================
// Application Settings
// ============================================================================

struct AppSettings {
    // General
    bool startMinimized = false;
    bool minimizeToTray = true;
    bool showNotifications = true;
    bool autoHashOnConnect = true;
    bool autoHashOnEject = true;
    
    // Security
    bool requireConfirmationForNew = true;
    bool requireConfirmationForModified = true;
    bool blockModifiedDevices = false;
    int defaultTrustLevel = 0;
    
    // Hashing
    QString hashAlgorithm = "SHA256";
    int hashBufferSizeKB = 1024;  // 1MB default
    bool useMemoryMapping = true;
    int maxConcurrentHashes = 1;
    
    // UI
    QString theme = "dark";
    bool animationsEnabled = true;
    int refreshIntervalMs = 1000;
    
    // Paths
    QString databasePath;
    QString logPath;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["start_minimized"] = startMinimized;
        obj["minimize_to_tray"] = minimizeToTray;
        obj["show_notifications"] = showNotifications;
        obj["auto_hash_on_connect"] = autoHashOnConnect;
        obj["auto_hash_on_eject"] = autoHashOnEject;
        obj["require_confirmation_new"] = requireConfirmationForNew;
        obj["require_confirmation_modified"] = requireConfirmationForModified;
        obj["block_modified_devices"] = blockModifiedDevices;
        obj["default_trust_level"] = defaultTrustLevel;
        obj["hash_algorithm"] = hashAlgorithm;
        obj["hash_buffer_size_kb"] = hashBufferSizeKB;
        obj["use_memory_mapping"] = useMemoryMapping;
        obj["max_concurrent_hashes"] = maxConcurrentHashes;
        obj["theme"] = theme;
        obj["animations_enabled"] = animationsEnabled;
        obj["refresh_interval_ms"] = refreshIntervalMs;
        obj["database_path"] = databasePath;
        obj["log_path"] = logPath;
        return obj;
    }
    
    static AppSettings fromJson(const QJsonObject& obj) {
        AppSettings settings;
        settings.startMinimized = obj["start_minimized"].toBool(false);
        settings.minimizeToTray = obj["minimize_to_tray"].toBool(true);
        settings.showNotifications = obj["show_notifications"].toBool(true);
        settings.autoHashOnConnect = obj["auto_hash_on_connect"].toBool(true);
        settings.autoHashOnEject = obj["auto_hash_on_eject"].toBool(true);
        settings.requireConfirmationForNew = obj["require_confirmation_new"].toBool(true);
        settings.requireConfirmationForModified = obj["require_confirmation_modified"].toBool(true);
        settings.blockModifiedDevices = obj["block_modified_devices"].toBool(false);
        settings.defaultTrustLevel = obj["default_trust_level"].toInt(0);
        settings.hashAlgorithm = obj["hash_algorithm"].toString("SHA256");
        settings.hashBufferSizeKB = obj["hash_buffer_size_kb"].toInt(1024);
        settings.useMemoryMapping = obj["use_memory_mapping"].toBool(true);
        settings.maxConcurrentHashes = obj["max_concurrent_hashes"].toInt(1);
        settings.theme = obj["theme"].toString("dark");
        settings.animationsEnabled = obj["animations_enabled"].toBool(true);
        settings.refreshIntervalMs = obj["refresh_interval_ms"].toInt(1000);
        settings.databasePath = obj["database_path"].toString();
        settings.logPath = obj["log_path"].toString();
        return settings;
    }
};

// ============================================================================
// Log Entry
// ============================================================================

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Security
};

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString category;
    QString message;
    QString deviceId;
    
    QString levelString() const {
        switch (level) {
            case LogLevel::Debug:    return "DEBUG";
            case LogLevel::Info:     return "INFO";
            case LogLevel::Warning:  return "WARN";
            case LogLevel::Error:    return "ERROR";
            case LogLevel::Security: return "SECURITY";
        }
        return "UNKNOWN";
    }
    
    QString toString() const {
        return QString("[%1] [%2] [%3] %4")
            .arg(timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(levelString(), -8)
            .arg(category, -12)
            .arg(message);
    }
};

} // namespace FlashSentry

// Register types for Qt's meta-object system
Q_DECLARE_METATYPE(FlashSentry::DeviceInfo)
Q_DECLARE_METATYPE(FlashSentry::DeviceRecord)
Q_DECLARE_METATYPE(FlashSentry::HashResult)
Q_DECLARE_METATYPE(FlashSentry::VerificationStatus)