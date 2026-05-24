#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMetaType>
#include <cstdint>

namespace FlashSentry {

struct DeviceInfo {
    QString deviceNode;
    QString parentDevice;
    QString serial;
    QString vendor;
    QString model;
    QString label;
    QString fsType;
    QString mountPoint;
    uint64_t sizeBytes = 0;
    bool isRemovable = true;
    bool isMounted = false;

    QString displayName() const {
        if (!label.isEmpty()) return label;
        if (!model.isEmpty()) return model;
        return deviceNode.split('/').last();
    }

    QString uniqueId() const {
        return serial.isEmpty() ? QString("%1_%2").arg(vendor, model)
                                : QString("%1_%2_%3").arg(serial, vendor, model);
    }

    QString legacyUniqueId() const {
        return uniqueId();
    }

    QString partitionUniqueId() const {
        const QString part = deviceNode.section('/', -1);
        return QString("%1_%2").arg(uniqueId(), part);
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

enum class VerificationProfile {
    WatchManifest,
    FullPartition,
    Hybrid
};

inline QString verificationProfileToString(VerificationProfile p) {
    switch (p) {
        case VerificationProfile::WatchManifest: return QStringLiteral("watch_manifest");
        case VerificationProfile::FullPartition: return QStringLiteral("full_partition");
        case VerificationProfile::Hybrid: return QStringLiteral("hybrid");
    }
    return QStringLiteral("watch_manifest");
}

inline VerificationProfile verificationProfileFromString(const QString& s) {
    if (s == QLatin1String("full_partition")) return VerificationProfile::FullPartition;
    if (s == QLatin1String("hybrid")) return VerificationProfile::Hybrid;
    return VerificationProfile::WatchManifest;
}

struct WatchFileEntry {
    QString relativePath;
    QString contentHash;
    uint64_t sizeBytes = 0;
    QDateTime modifiedUtc;
};

struct WatchGroup {
    QString id;
    QString name;
    QStringList watchPaths;
    QString merkleRoot;
    QList<WatchFileEntry> files;
    QDateTime builtAt;
};

struct WatchManifest {
    QString version = QStringLiteral("1.0");
    QList<WatchGroup> groups;
    QString manifestRoot;
    QDateTime updatedAt;

    bool hasBaseline() const {
        for (const WatchGroup& g : groups) {
            if (!g.merkleRoot.isEmpty()) return true;
        }
        return !manifestRoot.isEmpty();
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["version"] = version;
        QJsonArray ga;
        for (const WatchGroup& g : groups) {
            QJsonObject go;
            go["id"] = g.id;
            go["name"] = g.name;
            go["merkle_root"] = g.merkleRoot;
            go["built_at"] = g.builtAt.toString(Qt::ISODate);
            QJsonArray paths;
            for (const QString& p : g.watchPaths) paths.append(p);
            go["watch_paths"] = paths;
            QJsonArray files;
            for (const WatchFileEntry& f : g.files) {
                QJsonObject fo;
                fo["path"] = f.relativePath;
                fo["hash"] = f.contentHash;
                files.append(fo);
            }
            go["files"] = files;
            ga.append(go);
        }
        obj["groups"] = ga;
        obj["manifest_root"] = manifestRoot;
        obj["updated_at"] = updatedAt.toString(Qt::ISODate);
        return obj;
    }

    static WatchManifest fromJson(const QJsonObject& obj) {
        WatchManifest m;
        m.version = obj["version"].toString(QStringLiteral("1.0"));
        m.manifestRoot = obj["manifest_root"].toString();
        m.updatedAt = QDateTime::fromString(obj["updated_at"].toString(), Qt::ISODate);
        for (const QJsonValue& gv : obj["groups"].toArray()) {
            QJsonObject go = gv.toObject();
            WatchGroup g;
            g.id = go["id"].toString();
            g.name = go["name"].toString();
            g.merkleRoot = go["merkle_root"].toString();
            g.builtAt = QDateTime::fromString(go["built_at"].toString(), Qt::ISODate);
            for (const QJsonValue& pv : go["watch_paths"].toArray()) {
                g.watchPaths.append(pv.toString());
            }
            for (const QJsonValue& fv : go["files"].toArray()) {
                QJsonObject fo = fv.toObject();
                WatchFileEntry f;
                f.relativePath = fo["path"].toString();
                f.contentHash = fo["hash"].toString();
                g.files.append(f);
            }
            m.groups.append(g);
        }
        return m;
    }
};

struct ManifestVerifyResult {
    QString deviceNode;
    bool success = false;
    bool matches = false;
    QString computedRootHex;
    QString expectedRootHex;
    QStringList changedPaths;
    QStringList missingPaths;
    QStringList addedPaths;
    QString errorMessage;
    uint64_t filesChecked = 0;
    uint64_t durationMs = 0;
};

enum class AppModule { UsbMonitor, IsoVerifier };

inline AppModule appModuleFromString(const QString& s) {
    if (s == QLatin1String("iso_verifier") || s == QLatin1String("iso"))
        return AppModule::IsoVerifier;
    return AppModule::UsbMonitor;
}

inline QString appModuleToString(AppModule m) {
    return m == AppModule::IsoVerifier ? QStringLiteral("iso_verifier")
                                       : QStringLiteral("usb_monitor");
}

enum class IsoVerifySource { Unknown, LocalSidecar, RemotePublisher, ComputedOnly };

struct IsoVerifyResult {
    QString isoPath;
    QString mountPoint;
    QString deviceNode;
    QString publisherId;
    QString publisherName;
    QString releaseLabel;
    IsoVerifySource source = IsoVerifySource::Unknown;
    QString computedSha256;
    QString expectedSha256;
    bool hashChecked = false;
    bool hashMatches = false;
    bool pgpChecked = false;
    bool pgpValid = false;
    bool signatureCoversChecksums = false;
    bool fingerprintTrusted = false;
    QString signingKeyId;
    QString signingKeyFingerprint;
    QStringList trustedFingerprints;
    QString pgpSummary;
    QString checksumUrl;
    QString signatureUrl;
    QString keyserverUsed;
    bool remoteFetched = false;
    QString layoutNote;
    QString reportSummary;
    bool success = false;
    QString errorMessage;
    uint64_t durationMs = 0;

    bool passed() const {
        if (!success) return false;
        if (hashChecked && !expectedSha256.isEmpty() && !hashMatches) return false;
        if (pgpChecked && !pgpValid) return false;
        if (pgpChecked && !fingerprintTrusted) return false;
        return true;
    }
};

struct DeviceRecord {
    QString uniqueId;
    QString hash;
    QString hashAlgorithm = "SHA256";
    QDateTime firstSeen;
    QDateTime lastSeen;
    QDateTime lastHashed;
    uint64_t hashDurationMs = 0;
    int trustLevel = 0;
    bool autoMount = false;
    QString notes;
    DeviceInfo lastKnownInfo;
    VerificationProfile verificationProfile = VerificationProfile::WatchManifest;
    WatchManifest watchManifest;
    QString lastManifestRoot;

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
        obj["verification_profile"] = verificationProfileToString(verificationProfile);
        obj["watch_manifest"] = watchManifest.toJson();
        obj["last_manifest_root"] = lastManifestRoot;
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
        record.verificationProfile = verificationProfileFromString(
            obj["verification_profile"].toString());
        record.watchManifest = WatchManifest::fromJson(obj["watch_manifest"].toObject());
        record.lastManifestRoot = obj["last_manifest_root"].toString();
        return record;
    }
};

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

enum class VerificationStatus {
    Unknown,
    Pending,
    Hashing,
    Verified,
    Modified,
    NewDevice,
    Error
};

inline QString verificationStatusToString(VerificationStatus status) {
    switch (status) {
        case VerificationStatus::Unknown: return "Unknown";
        case VerificationStatus::Pending: return "Pending";
        case VerificationStatus::Hashing: return "Hashing...";
        case VerificationStatus::Verified: return "Verified";
        case VerificationStatus::Modified: return "MODIFIED";
        case VerificationStatus::NewDevice: return "New Device";
        case VerificationStatus::Error: return "Error";
    }
    return "Unknown";
}

struct AppSettings {
    bool startMinimized = false;
    bool minimizeToTray = true;
    bool showNotifications = true;
    bool autoHashOnConnect = false;
    bool autoHashOnEject = true;
    bool requireConfirmationForNew = true;
    bool requireConfirmationForModified = true;
    bool blockModifiedDevices = false;
    int defaultTrustLevel = 0;
    QString hashAlgorithm = "SHA256";
    int hashBufferSizeKB = 1024;
    bool useMemoryMapping = true;
    int maxConcurrentHashes = 1;
    QString theme = "dark";
    bool animationsEnabled = true;
    int refreshIntervalMs = 1000;
    QString databasePath;
    QString logPath;
    AppModule appModule = AppModule::UsbMonitor;
    VerificationProfile defaultVerificationProfile = VerificationProfile::WatchManifest;
    QString isoScanDirectory;
    bool isoAutoVerifyOnScan = true;
    bool isoAutoVerifyOnUsbMount = true;
    bool promptPerPartition = false;

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
        obj["app_module"] = static_cast<int>(appModule);
        obj["default_verification_profile"] = verificationProfileToString(defaultVerificationProfile);
        obj["iso_scan_directory"] = isoScanDirectory;
        obj["iso_auto_verify_on_scan"] = isoAutoVerifyOnScan;
        obj["iso_auto_verify_on_usb_mount"] = isoAutoVerifyOnUsbMount;
        return obj;
    }

    static AppSettings fromJson(const QJsonObject& obj) {
        AppSettings settings;
        settings.startMinimized = obj["start_minimized"].toBool(false);
        settings.minimizeToTray = obj["minimize_to_tray"].toBool(true);
        settings.showNotifications = obj["show_notifications"].toBool(true);
        settings.autoHashOnConnect = obj["auto_hash_on_connect"].toBool(false);
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
        settings.appModule = static_cast<AppModule>(obj["app_module"].toInt(0));
        settings.defaultVerificationProfile = verificationProfileFromString(
            obj["default_verification_profile"].toString());
        settings.isoScanDirectory = obj["iso_scan_directory"].toString();
        settings.isoAutoVerifyOnScan = obj["iso_auto_verify_on_scan"].toBool(true);
        settings.isoAutoVerifyOnUsbMount = obj["iso_auto_verify_on_usb_mount"].toBool(true);
        return settings;
    }
};

enum class LogLevel { Debug, Info, Warning, Error, Security };

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString category;
    QString message;
    QString deviceId;

    QString levelString() const {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
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

Q_DECLARE_METATYPE(FlashSentry::DeviceInfo)
Q_DECLARE_METATYPE(FlashSentry::DeviceRecord)
Q_DECLARE_METATYPE(FlashSentry::HashResult)
Q_DECLARE_METATYPE(FlashSentry::VerificationStatus)
Q_DECLARE_METATYPE(FlashSentry::ManifestVerifyResult)
Q_DECLARE_METATYPE(FlashSentry::WatchManifest)
Q_DECLARE_METATYPE(FlashSentry::IsoVerifyResult)
