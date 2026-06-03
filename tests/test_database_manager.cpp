#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QProcessEnvironment>

#include "DatabaseManager.h"
#include "policy/PolicyGateway.h"
#include "policy/PolicyServiceLocator.h"

using namespace FlashSpartan;

namespace {

void installIsolatedPolicy(const QString& configDir)
{
    qputenv("FLASHSPARTAN_POLICY_IN_PROCESS", "1");
    qputenv("FLASHSPARTAN_POLICY_CONFIG", configDir.toUtf8());
    Policy::PolicyServiceLocator::install(Policy::PolicyGateway::createInProcess());
}

} // namespace

class TestDatabaseManager : public QObject {
    Q_OBJECT

private slots:
    void partitionAwareLookupAndMigration();
    void verifyHashUsesLegacyId();
    void getDeviceReturnsLegacyRecord();
    void updateLastSeenOnLegacyId();
    void importMergeAndReplace();
};

void TestDatabaseManager::partitionAwareLookupAndMigration()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo info;
    info.serial = "SER001";
    info.vendor = "Vendor";
    info.model = "Model";
    info.deviceNode = "/dev/sdc2";

    DeviceRecord legacy;
    legacy.uniqueId = info.legacyUniqueId();
    legacy.hash = "abc";
    legacy.hashAlgorithm = "SHA256";
    legacy.firstSeen = QDateTime::currentDateTimeUtc();
    legacy.lastSeen = legacy.firstSeen;
    legacy.lastKnownInfo = info;
    QVERIFY(db.addDevice(legacy));

    QVERIFY(db.hasDevice(info));
    const QString canonical = db.canonicalUniqueId(info);
    QCOMPARE(canonical, info.partitionUniqueId());
    QVERIFY(db.hasDevice(canonical));
    QVERIFY(db.hasDevice(info.legacyUniqueId()));
}

void TestDatabaseManager::verifyHashUsesLegacyId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo info;
    info.serial = "SER002";
    info.vendor = "Vendor";
    info.model = "Model";
    info.deviceNode = "/dev/sdd1";

    DeviceRecord legacy;
    legacy.uniqueId = info.legacyUniqueId();
    legacy.hash = "0123abcd";
    legacy.hashAlgorithm = "SHA256";
    legacy.firstSeen = QDateTime::currentDateTimeUtc();
    legacy.lastSeen = legacy.firstSeen;
    QVERIFY(db.addDevice(legacy));

    QVERIFY(db.verifyHash(info, "0123abcd"));
    QVERIFY(!db.verifyHash(info, "ffff"));
}

void TestDatabaseManager::getDeviceReturnsLegacyRecord()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo info;
    info.serial = "SER003";
    info.vendor = "Vendor";
    info.model = "Model";
    info.deviceNode = "/dev/sde2";

    DeviceRecord legacy;
    legacy.uniqueId = info.legacyUniqueId();
    legacy.hash = "deadbeef";
    legacy.hashAlgorithm = "SHA256";
    legacy.firstSeen = QDateTime::currentDateTimeUtc();
    legacy.lastSeen = legacy.firstSeen;
    QVERIFY(db.addDevice(legacy));

    const auto record = db.getDevice(info);
    QVERIFY(record.has_value());
    QCOMPARE(record->uniqueId, legacy.uniqueId);
    QCOMPARE(record->hash, legacy.hash);
}

void TestDatabaseManager::updateLastSeenOnLegacyId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo info;
    info.serial = "SER004";
    info.vendor = "Vendor";
    info.model = "Model";
    info.deviceNode = "/dev/sdf3";

    DeviceRecord legacy;
    legacy.uniqueId = info.legacyUniqueId();
    legacy.hash = "abc123";
    legacy.hashAlgorithm = "SHA256";
    legacy.firstSeen = QDateTime::currentDateTimeUtc().addDays(-1);
    legacy.lastSeen = legacy.firstSeen;
    QVERIFY(db.addDevice(legacy));

    const QDateTime before = QDateTime::currentDateTimeUtc();
    QVERIFY(db.updateLastSeen(db.canonicalUniqueId(info)));
    const auto record = db.getDevice(info);
    QVERIFY(record.has_value());
    QVERIFY(record->lastSeen >= before);
}

void TestDatabaseManager::importMergeAndReplace()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QTemporaryDir seedDir;
    QVERIFY(seedDir.isValid());
    installIsolatedPolicy(seedDir.path());

    DatabaseManager seedDb;
    QVERIFY(seedDb.initialize());

    DeviceRecord second;
    second.uniqueId = "device_b/sdb1";
    second.hash = "hash_b";
    second.firstSeen = QDateTime::currentDateTimeUtc();
    second.lastSeen = second.firstSeen;
    QVERIFY(seedDb.addDevice(second));

    const QString importPath = tempDir.path() + "/import.json";
    QVERIFY(seedDb.exportToFile(importPath, true));

    installIsolatedPolicy(tempDir.path());
    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceRecord first;
    first.uniqueId = "device_a/sda1";
    first.hash = "hash_a";
    first.firstSeen = QDateTime::currentDateTimeUtc();
    first.lastSeen = first.firstSeen;
    QVERIFY(db.addDevice(first));

    QCOMPARE(db.importFromFile(importPath, true), 1);
    QCOMPARE(db.deviceCount(), 2);

    QCOMPARE(db.importFromFile(importPath, false), 1);
    QCOMPARE(db.deviceCount(), 1);
    QVERIFY(db.hasDevice("device_b/sdb1"));
}

QTEST_MAIN(TestDatabaseManager)
#include "test_database_manager.moc"
