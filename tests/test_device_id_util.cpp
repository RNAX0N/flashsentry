#include <QtTest>

#include "DeviceIdUtil.h"

using namespace FlashSpartan;

class TestDeviceIdUtil : public QObject {
    Q_OBJECT

private slots:
    void linuxPartitionSuffix();
    void windowsDriveSuffix();
    void resolveLegacyRecord();
};

void TestDeviceIdUtil::linuxPartitionSuffix()
{
    QVERIFY(DeviceIdUtil::isPartitionNodeSuffix(QStringLiteral("sdb1")));
    QVERIFY(DeviceIdUtil::isPartitionNodeSuffix(QStringLiteral("mmcblk0p1")));
}

void TestDeviceIdUtil::windowsDriveSuffix()
{
    QVERIFY(DeviceIdUtil::isPartitionNodeSuffix(QStringLiteral("E")));
    QVERIFY(!DeviceIdUtil::isPartitionNodeSuffix(QStringLiteral("not-a-drive")));
}

void TestDeviceIdUtil::resolveLegacyRecord()
{
    QHash<QString, DeviceRecord> devices;
    DeviceRecord legacy;
    legacy.uniqueId = QStringLiteral("SER_Vendor_Model");
    devices.insert(legacy.uniqueId, legacy);

    const auto resolved =
        DeviceIdUtil::resolveStoredId(devices, QStringLiteral("SER_Vendor_Model_sdb1"));
    QVERIFY(resolved.has_value());
    QCOMPARE(*resolved, legacy.uniqueId);
}

QTEST_MAIN(TestDeviceIdUtil)
#include "test_device_id_util.moc"
