#include <QtTest>
#include <QTemporaryDir>
#include <QProcessEnvironment>

#include "DeviceDriveUtil.h"
#include "DeviceWhitelistService.h"
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

class TestDeviceDriveUtil : public QObject {
    Q_OBJECT

private slots:
    void driveKeyUsesParentDevice();
    void summarizeDriveCountsPartitions();
    void isDriveKnownWhenAnyPartitionTrusted();
};

void TestDeviceDriveUtil::driveKeyUsesParentDevice()
{
    DeviceInfo info;
    info.parentDevice = QStringLiteral("/dev/sdb");
    info.deviceNode = QStringLiteral("/dev/sdb1");
    QCOMPARE(DeviceDriveUtil::driveKey(info), QStringLiteral("/dev/sdb"));
}

void TestDeviceDriveUtil::summarizeDriveCountsPartitions()
{
    DeviceInfo a;
    a.parentDevice = QStringLiteral("/dev/sdb");
    a.deviceNode = QStringLiteral("/dev/sdb1");
    DeviceInfo b = a;
    b.deviceNode = QStringLiteral("/dev/sdb2");

    const auto summary = DeviceDriveUtil::summarizeDrive(a, {a, b});
    QCOMPARE(summary.partitionCount, 2);
    QCOMPARE(summary.partitionNodes.size(), 2);
}

void TestDeviceDriveUtil::isDriveKnownWhenAnyPartitionTrusted()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo part1;
    part1.serial = QStringLiteral("SER");
    part1.vendor = QStringLiteral("V");
    part1.model = QStringLiteral("M");
    part1.parentDevice = QStringLiteral("/dev/sdc");
    part1.deviceNode = QStringLiteral("/dev/sdc1");

    DeviceInfo part2 = part1;
    part2.deviceNode = QStringLiteral("/dev/sdc2");

    DeviceRecord record = DeviceWhitelistService::makeRecord(part1, db, 1);
    QVERIFY(db.addDevice(record));

    QVERIFY(DeviceDriveUtil::isDriveKnown(part2, {part1, part2}, db));
}

QTEST_MAIN(TestDeviceDriveUtil)
#include "test_device_drive_util.moc"
