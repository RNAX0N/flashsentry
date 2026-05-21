#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "DatabaseManager.h"

using namespace FlashSentry;

class TestDatabaseManager : public QObject {
    Q_OBJECT

private slots:
    void partitionAwareLookupAndMigration();
    void verifyHashUsesLegacyId();
    void importMergeAndReplace();
};

void TestDatabaseManager::partitionAwareLookupAndMigration()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager db;
    const QString dbPath = tempDir.path() + "/devices.json";
    QVERIFY(db.initialize(dbPath));

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
    QCOMPARE(canonical, info.uniqueId());
    QVERIFY(db.hasDevice(canonical));
    QVERIFY(!db.hasDevice(info.legacyUniqueId()));
}

void TestDatabaseManager::verifyHashUsesLegacyId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.path() + "/devices.json"));

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

void TestDatabaseManager::importMergeAndReplace()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    DatabaseManager db;
    QVERIFY(db.initialize(tempDir.path() + "/devices.json"));

    DeviceRecord first;
    first.uniqueId = "device_a/sda1";
    first.hash = "hash_a";
    first.firstSeen = QDateTime::currentDateTimeUtc();
    first.lastSeen = first.firstSeen;
    QVERIFY(db.addDevice(first));

    const QString importPath = tempDir.path() + "/import.json";
    DatabaseManager exporter;
    QVERIFY(exporter.initialize(importPath));

    DeviceRecord second;
    second.uniqueId = "device_b/sdb1";
    second.hash = "hash_b";
    second.firstSeen = QDateTime::currentDateTimeUtc();
    second.lastSeen = second.firstSeen;
    QVERIFY(exporter.addDevice(second));
    QVERIFY(exporter.save());

    QCOMPARE(db.importFromFile(importPath, true), 1);
    QCOMPARE(db.deviceCount(), 2);

    QCOMPARE(db.importFromFile(importPath, false), 1);
    QCOMPARE(db.deviceCount(), 1);
    QVERIFY(db.hasDevice("device_b/sdb1"));
}

QTEST_MAIN(TestDatabaseManager)
#include "test_database_manager.moc"
