#include <QtTest>
#include "Types.h"

using namespace FlashSpartan;

class TestTypes : public QObject {
    Q_OBJECT

private slots:
    void partitionUniqueIdIncludesPartition();
    void legacyUniqueIdOmitsPartition();
    void deviceRecordJsonRoundTrip();
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
