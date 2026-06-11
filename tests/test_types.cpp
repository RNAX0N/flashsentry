#include <QtTest>
#include "Types.h"

using namespace FlashSpartan;

class TestTypes : public QObject {
    Q_OBJECT

private slots:
    void partitionUniqueIdIncludesPartition();
    void legacyUniqueIdOmitsPartition();
    void deviceRecordJsonRoundTrip();
    void isoVerifyPassedRequiresTrustedComparison();
    void isoVerifyComputedOnlyIsInconclusive();
    void isoVerifyLayoutNoteIsInformational();
    void deviceWeakIdentityWhenSerialMissing();
};

void TestTypes::partitionUniqueIdIncludesPartition()
{
    DeviceInfo info;
    info.serial = "ABC123";
    info.vendor = "SanDisk";
    info.model = "Ultra";
    info.deviceNode = "/dev/sdb1";

    QCOMPARE(info.uniqueId(), QString("ABC123_SanDisk_Ultra"));
    QCOMPARE(info.partitionUniqueId(), QString("ABC123_SanDisk_Ultra_sdb1"));
}

void TestTypes::legacyUniqueIdOmitsPartition()
{
    DeviceInfo info;
    info.serial = "ABC123";
    info.vendor = "SanDisk";
    info.model = "Ultra";
    info.deviceNode = "/dev/sdb1";

    QCOMPARE(info.legacyUniqueId(), QString("ABC123_SanDisk_Ultra"));
}

void TestTypes::isoVerifyPassedRequiresTrustedComparison()
{
    IsoVerifyResult r;
    r.success = true;
    r.isoPath = QStringLiteral("/tmp/debian.iso");
    r.hashChecked = true;
    r.expectedSha256 = QStringLiteral("ab");
    r.hashMatches = true;
    QVERIFY(r.passed());
    QVERIFY(!r.inconclusive());
}

void TestTypes::isoVerifyComputedOnlyIsInconclusive()
{
    IsoVerifyResult r;
    r.success = true;
    r.isoPath = QStringLiteral("/tmp/unknown.iso");
    r.hashChecked = true;
    r.computedSha256 = QStringLiteral("deadbeef");
    r.source = IsoVerifySource::ComputedOnly;
    QVERIFY(r.inconclusive());
    QVERIFY(!r.passed());
}

void TestTypes::isoVerifyLayoutNoteIsInformational()
{
    IsoVerifyResult r;
    r.success = true;
    r.layoutNote = QStringLiteral("Live USB layout detected");
    r.source = IsoVerifySource::Unknown;
    QVERIFY(!r.inconclusive());
    QVERIFY(r.passed());
}

void TestTypes::deviceWeakIdentityWhenSerialMissing()
{
    DeviceInfo weak;
    weak.vendor = QStringLiteral("Generic");
    weak.model = QStringLiteral("Flash");
    weak.deviceNode = QStringLiteral("/dev/sdb1");
    QVERIFY(weak.hasWeakIdentity());
    QCOMPARE(weak.uniqueId(), QStringLiteral("Generic_Flash"));
    QCOMPARE(weak.partitionUniqueId(), QStringLiteral("Generic_Flash_sdb1"));
    QVERIFY(!weak.weakIdentitySummary().isEmpty());

    DeviceInfo strong = weak;
    strong.serial = QStringLiteral("SER1");
    QVERIFY(!strong.hasWeakIdentity());
    QCOMPARE(strong.uniqueId(), QStringLiteral("SER1_Generic_Flash"));
}

void TestTypes::deviceRecordJsonRoundTrip()
{
    DeviceRecord record;
    record.uniqueId = "ABC123_SanDisk_Ultra/sdb1";
    record.hash = "deadbeef";
    record.hashAlgorithm = "SHA256";
    record.firstSeen = QDateTime::fromString("2024-01-01T00:00:00Z", Qt::ISODate);
    record.trustLevel = 1;

    const DeviceRecord restored = DeviceRecord::fromJson(record.toJson());
    QCOMPARE(restored.uniqueId, record.uniqueId);
    QCOMPARE(restored.hash, record.hash);
    QCOMPARE(restored.hashAlgorithm, record.hashAlgorithm);
    QCOMPARE(restored.trustLevel, record.trustLevel);
}

QTEST_MAIN(TestTypes)
#include "test_types.moc"
