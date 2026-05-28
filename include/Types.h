#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMetaType>
#include <QCryptographicHash>
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


enum class HashScope {
    Partition,
    WholeDisk,
};

enum class HashScanMode {
    Full,
    QuickSample,
    WatchManifestOnly,
};

inline QString hashScopeToString(HashScope s) {
    return s == HashScope::WholeDisk ? QStringLiteral("whole_disk") : QStringLiteral("partition");
}

inline HashScope hashScopeFromString(const QString& s) {
    if (s == QLatin1String("whole_disk")) return HashScope::WholeDisk;
    return HashScope::Partition;
}

inline QString hashScanModeToString(HashScanMode m) {
    switch (m) {
        case HashScanMode::QuickSample: return QStringLiteral("quick");
        case HashScanMode::WatchManifestOnly: return QStringLiteral("watch");
        case HashScanMode::Full:
        default: return QStringLiteral("full");
    }
}

inline HashScanMode hashScanModeFromString(const QString& s) {
    if (s == QLatin1String("quick")) return HashScanMode::QuickSample;
    if (s == QLatin1String("watch")) return HashScanMode::WatchManifestOnly;
    return HashScanMode::Full;
}

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

struct HidInterfaceInfo {
    QString number;
    QString interfaceClass;
    QString interfaceSubClass;
    QString interfaceProtocol;
    QString driver;

    QString signature() const {
        return QStringLiteral("%1:%2:%3:%4:%5")
            .arg(number, interfaceClass, interfaceSubClass, interfaceProtocol, driver);
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["number"] = number;
        obj["class"] = interfaceClass;
        obj["subclass"] = interfaceSubClass;
        obj["protocol"] = interfaceProtocol;
        obj["driver"] = driver;
        return obj;
    }

    static HidInterfaceInfo fromJson(const QJsonObject& obj) {
        HidInterfaceInfo info;
        info.number = obj["number"].toString();
        info.interfaceClass = obj["class"].toString();
        info.interfaceSubClass = obj["subclass"].toString();
        info.interfaceProtocol = obj["protocol"].toString();
        info.driver = obj["driver"].toString();
        return info;
    }
};

struct HidDeviceInfo {
    QString sysPath;
    QString devNode;
    QString usbPath;
    QString usbBus;
    QString usbPort;
    QString vendorId;
    QString productId;
    QString serial;
    QString manufacturer;
    QString product;
    QString driver;
    QStringList capabilities;
    QList<HidInterfaceInfo> interfaces;
    QDateTime seenAtUtc;

    QString displayName() const {
        if (!product.isEmpty()) return product;
        if (!manufacturer.isEmpty()) return manufacturer;
        if (!devNode.isEmpty()) return devNode;
        return QStringLiteral("USB HID device");
    }

    bool isKeyboard() const {
        return capabilities.contains(QStringLiteral("keyboard"));
    }

    bool isMouse() const {
        return capabilities.contains(QStringLiteral("mouse"));
    }

    QString stableId() const {
        const QString identity = QStringList{vendorId, productId, serial, usbPath}.join(QLatin1Char(':'));
        if (!vendorId.isEmpty() && !productId.isEmpty()
            && (!serial.isEmpty() || !usbPath.isEmpty())) {
            return identity;
        }
        const QByteArray digest =
            QCryptographicHash::hash((identity + sysPath + devNode).toUtf8(),
                                     QCryptographicHash::Sha256).toHex();
        return QStringLiteral("hid:%1").arg(QString::fromLatin1(digest.left(16)));
    }

    QStringList interfaceSignatures() const {
        QStringList out;
        for (const HidInterfaceInfo& iface : interfaces) {
            out.append(iface.signature());
        }
        out.sort();
        out.removeDuplicates();
        return out;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["stable_id"] = stableId();
        obj["sys_path"] = sysPath;
        obj["dev_node"] = devNode;
        obj["usb_path"] = usbPath;
        obj["usb_bus"] = usbBus;
        obj["usb_port"] = usbPort;
        obj["vendor_id"] = vendorId;
        obj["product_id"] = productId;
        obj["serial"] = serial;
        obj["manufacturer"] = manufacturer;
        obj["product"] = product;
        obj["driver"] = driver;
        QJsonArray caps;
        for (const QString& cap : capabilities) caps.append(cap);
        obj["capabilities"] = caps;
        QJsonArray ifaces;
        for (const HidInterfaceInfo& iface : interfaces) ifaces.append(iface.toJson());
        obj["interfaces"] = ifaces;
        obj["seen_at"] = seenAtUtc.toString(Qt::ISODate);
        return obj;
    }

    static HidDeviceInfo fromJson(const QJsonObject& obj) {
        HidDeviceInfo info;
        info.sysPath = obj["sys_path"].toString();
        info.devNode = obj["dev_node"].toString();
        info.usbPath = obj["usb_path"].toString();
        info.usbBus = obj["usb_bus"].toString();
        info.usbPort = obj["usb_port"].toString();
        info.vendorId = obj["vendor_id"].toString();
        info.productId = obj["product_id"].toString();
        info.serial = obj["serial"].toString();
        info.manufacturer = obj["manufacturer"].toString();
        info.product = obj["product"].toString();
        info.driver = obj["driver"].toString();
        for (const QJsonValue& val : obj["capabilities"].toArray()) {
            const QString cap = val.toString();
            if (!cap.isEmpty()) info.capabilities.append(cap);
        }
        info.capabilities.removeDuplicates();
        for (const QJsonValue& val : obj["interfaces"].toArray()) {
            info.interfaces.append(HidInterfaceInfo::fromJson(val.toObject()));
        }
        info.seenAtUtc = QDateTime::fromString(obj["seen_at"].toString(), Qt::ISODate);
        return info;
    }
};

struct BadUsbBaselineEntry {
    QString stableId;
    HidDeviceInfo device;
    bool trusted = false;
    QDateTime firstSeenUtc;
    QDateTime lastSeenUtc;
    QString notes;

    QJsonObject toJson() const {
        QJsonObject obj = device.toJson();
        obj["stable_id"] = stableId.isEmpty() ? device.stableId() : stableId;
        obj["trusted"] = trusted;
        obj["first_seen"] = firstSeenUtc.toString(Qt::ISODate);
        obj["last_seen"] = lastSeenUtc.toString(Qt::ISODate);
        obj["notes"] = notes;
        return obj;
    }

    static BadUsbBaselineEntry fromJson(const QJsonObject& obj) {
        BadUsbBaselineEntry entry;
        entry.device = HidDeviceInfo::fromJson(obj);
        entry.stableId = obj["stable_id"].toString(entry.device.stableId());
        entry.trusted = obj["trusted"].toBool(false);
        entry.firstSeenUtc = QDateTime::fromString(obj["first_seen"].toString(), Qt::ISODate);
        entry.lastSeenUtc = QDateTime::fromString(obj["last_seen"].toString(), Qt::ISODate);
        entry.notes = obj["notes"].toString();
        return entry;
    }
};

enum class BadUsbSeverity {
    Info,
    Warning,
    Critical
};

struct BadUsbAnomalyResult {
    bool anomalous = false;
    BadUsbSeverity severity = BadUsbSeverity::Info;
    QString ruleId;
    QString summary;
    QString detail;
    HidDeviceInfo device;
    QStringList relatedStorageNodes;
    QDateTime detectedAtUtc;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["anomalous"] = anomalous;
        obj["severity"] = static_cast<int>(severity);
        obj["rule_id"] = ruleId;
        obj["summary"] = summary;
        obj["detail"] = detail;
        obj["device"] = device.toJson();
        QJsonArray storage;
        for (const QString& node : relatedStorageNodes) storage.append(node);
        obj["related_storage_nodes"] = storage;
        obj["detected_at"] = detectedAtUtc.toString(Qt::ISODate);
        return obj;
    }
};

enum class AppModule { UsbMonitor, IsoVerifier, BadUsbMonitor };

inline AppModule appModuleFromString(const QString& s) {
    if (s == QLatin1String("iso_verifier") || s == QLatin1String("iso"))
        return AppModule::IsoVerifier;
    if (s == QLatin1String("badusb_monitor") || s == QLatin1String("badusb"))
        return AppModule::BadUsbMonitor;
    return AppModule::UsbMonitor;
}

inline QString appModuleToString(AppModule m) {
    switch (m) {
        case AppModule::IsoVerifier: return QStringLiteral("iso_verifier");
        case AppModule::BadUsbMonitor: return QStringLiteral("badusb_monitor");
        case AppModule::UsbMonitor: return QStringLiteral("usb_monitor");
    }
    return QStringLiteral("usb_monitor");
}

enum class IsoVerifySource {
    Unknown,
    LocalSidecar,
    RemotePublisher,
    EmbeddedCatalog,
    ComputedOnly
};

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
    QString hashScope;
    QString hashScanMode;
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
        obj["hash_scope"] = hashScope;
        obj["hash_scan_mode"] = hashScanMode;
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
        record.hashScope = obj["hash_scope"].toString();
        record.hashScanMode = obj["hash_scan_mode"].toString();
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
    QString hashScopeLabel;
    QString scanModeLabel;
    bool resumedFromCheckpoint = false;

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
        case VerificationStatus::Verified: return QStringLiteral("VERIFIED");
        case VerificationStatus::Modified: return "MODIFIED";
        case VerificationStatus::NewDevice: return "New Device";
        case VerificationStatus::Error: return "Error";
    }
    return "Unknown";
}

struct AppSettings {
    bool startMinimized = false;
    bool autoStartAtLogin = false;
    bool minimizeToTray = true;
    bool showNotifications = true;
    bool autoHashOnConnect = false;
    bool autoHashOnEject = true;
    bool requireConfirmationForNew = true;
    bool requireConfirmationForModified = true;
    bool blockModifiedDevices = false;
    int defaultTrustLevel = 0;
    QString hashAlgorithm = "SHA256";
    QString hashScope;
    QString hashScanMode;
    int hashBufferSizeKB = 1024;
    bool useMemoryMapping = true;
    int maxConcurrentHashes = 1;
    QString theme = "dark";
    bool animationsEnabled = true;
    int fontSizePt = 10;
    int refreshIntervalMs = 1000;
    QString databasePath;
    QString logPath;
    AppModule appModule = AppModule::UsbMonitor;
    VerificationProfile defaultVerificationProfile = VerificationProfile::WatchManifest;
    QString isoScanDirectory;
    bool isoAutoVerifyOnScan = true;
    bool isoAutoVerifyOnUsbMount = true;
    bool promptPerPartition = false;
    HashScope defaultHashScope = HashScope::Partition;
    HashScanMode defaultHashScanMode = HashScanMode::Full;
    bool hashResumeCheckpoints = true;
    bool promptHashOptionsOnManual = true;
    bool blockMountOnIsoVerifyFailure = false;
    bool isoVerifyDecompressed = false;
    bool isoPreferOfflineSidecars = false;
    int isoVerifyParallel = 2;
    bool showFirstRunWizard = true;
    QString settingsProfile = QStringLiteral("default");
    bool badUsbEnabled = true;
    bool badUsbAlertNewKeyboard = true;
    bool badUsbAlertCompositeStorage = true;
    bool badUsbAlertInterfaceDrift = true;
    bool badUsbAlertRapidReconnect = true;
    bool badUsbAutoBaselineTrusted = false;
    bool badUsbConfirmAnomalies = true;
    bool badUsbUsbmonEnabled = false;
    bool badUsbUsbmonOnAnomalyOnly = true;
    QString badUsbUsbmonCommand =
        QStringLiteral("tcpdump -i usbmon{bus} -w {out} -G 30 -W 1");

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["start_minimized"] = startMinimized;
        obj["auto_start_at_login"] = autoStartAtLogin;
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
        obj["font_size_pt"] = fontSizePt;
        obj["refresh_interval_ms"] = refreshIntervalMs;
        obj["database_path"] = databasePath;
        obj["log_path"] = logPath;
        obj["app_module"] = static_cast<int>(appModule);
        obj["default_verification_profile"] = verificationProfileToString(defaultVerificationProfile);
        obj["iso_scan_directory"] = isoScanDirectory;
        obj["iso_auto_verify_on_scan"] = isoAutoVerifyOnScan;
        obj["iso_auto_verify_on_usb_mount"] = isoAutoVerifyOnUsbMount;
        obj["block_mount_on_iso_failure"] = blockMountOnIsoVerifyFailure;
        obj["iso_verify_decompressed"] = isoVerifyDecompressed;
        obj["iso_prefer_offline_sidecars"] = isoPreferOfflineSidecars;
        obj["iso_verify_parallel"] = isoVerifyParallel;
        obj["show_first_run_wizard"] = showFirstRunWizard;
        obj["settings_profile"] = settingsProfile;
        obj["badusb_enabled"] = badUsbEnabled;
        obj["badusb_alert_new_keyboard"] = badUsbAlertNewKeyboard;
        obj["badusb_alert_composite_storage"] = badUsbAlertCompositeStorage;
        obj["badusb_alert_interface_drift"] = badUsbAlertInterfaceDrift;
        obj["badusb_alert_rapid_reconnect"] = badUsbAlertRapidReconnect;
        obj["badusb_auto_baseline_trusted"] = badUsbAutoBaselineTrusted;
        obj["badusb_confirm_anomalies"] = badUsbConfirmAnomalies;
        obj["badusb_usbmon_enabled"] = badUsbUsbmonEnabled;
        obj["badusb_usbmon_on_anomaly_only"] = badUsbUsbmonOnAnomalyOnly;
        obj["badusb_usbmon_command"] = badUsbUsbmonCommand;
        return obj;
    }

    static AppSettings fromJson(const QJsonObject& obj) {
        AppSettings settings;
        settings.startMinimized = obj["start_minimized"].toBool(false);
        settings.autoStartAtLogin = obj["auto_start_at_login"].toBool(false);
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
        settings.fontSizePt = obj["font_size_pt"].toInt(10);
        settings.refreshIntervalMs = obj["refresh_interval_ms"].toInt(1000);
        settings.databasePath = obj["database_path"].toString();
        settings.logPath = obj["log_path"].toString();
        settings.appModule = static_cast<AppModule>(obj["app_module"].toInt(0));
        settings.defaultVerificationProfile = verificationProfileFromString(
            obj["default_verification_profile"].toString());
        settings.isoScanDirectory = obj["iso_scan_directory"].toString();
        settings.isoAutoVerifyOnScan = obj["iso_auto_verify_on_scan"].toBool(true);
        settings.isoAutoVerifyOnUsbMount = obj["iso_auto_verify_on_usb_mount"].toBool(true);
        settings.blockMountOnIsoVerifyFailure = obj["block_mount_on_iso_failure"].toBool(false);
        settings.isoVerifyDecompressed = obj["iso_verify_decompressed"].toBool(false);
        settings.isoPreferOfflineSidecars = obj["iso_prefer_offline_sidecars"].toBool(false);
        settings.isoVerifyParallel = obj["iso_verify_parallel"].toInt(2);
        settings.showFirstRunWizard = obj["show_first_run_wizard"].toBool(true);
        {
            QString profile = obj["settings_profile"].toString(QStringLiteral("default"));
            if (profile == QStringLiteral("ventoy")) {
                profile = QStringLiteral("multi_image");
            }
            settings.settingsProfile = profile;
        }
        settings.badUsbEnabled = obj["badusb_enabled"].toBool(true);
        settings.badUsbAlertNewKeyboard = obj["badusb_alert_new_keyboard"].toBool(true);
        settings.badUsbAlertCompositeStorage = obj["badusb_alert_composite_storage"].toBool(true);
        settings.badUsbAlertInterfaceDrift = obj["badusb_alert_interface_drift"].toBool(true);
        settings.badUsbAlertRapidReconnect = obj["badusb_alert_rapid_reconnect"].toBool(true);
        settings.badUsbAutoBaselineTrusted = obj["badusb_auto_baseline_trusted"].toBool(false);
        settings.badUsbConfirmAnomalies = obj["badusb_confirm_anomalies"].toBool(true);
        settings.badUsbUsbmonEnabled = obj["badusb_usbmon_enabled"].toBool(false);
        settings.badUsbUsbmonOnAnomalyOnly = obj["badusb_usbmon_on_anomaly_only"].toBool(true);
        settings.badUsbUsbmonCommand = obj["badusb_usbmon_command"].toString(
            QStringLiteral("tcpdump -i usbmon{bus} -w {out} -G 30 -W 1"));
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
Q_DECLARE_METATYPE(FlashSentry::HidDeviceInfo)
Q_DECLARE_METATYPE(FlashSentry::BadUsbAnomalyResult)
