#include <QtTest>
#include <QTemporaryDir>
#include <QProcessEnvironment>

#include "DeviceTrustCoordinator.h"
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

class TestDeviceTrustCoordinator : public QObject {
    Q_OBJECT

private slots:
    void planPromptDriveForUnknownDrive();
    void planSkipWhenDrivePromptInProgress();
    void whitelistDriveAddsMissingPartitions();
};

void TestDeviceTrustCoordinator::planPromptDriveForUnknownDrive()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo device;
    device.parentDevice = QStringLiteral("/dev/sdd");
    device.deviceNode = QStringLiteral("/dev/sdd1");

    const auto plan = DeviceTrustCoordinator::planNewDevice(device, false, true, false, {device},
                                                            db);
    QCOMPARE(plan.action, DeviceTrustCoordinator::NewDeviceAction::PromptDrive);
}

void TestDeviceTrustCoordinator::planSkipWhenDrivePromptInProgress()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo device;
    device.parentDevice = QStringLiteral("/dev/sde");
    device.deviceNode = QStringLiteral("/dev/sde1");

    const auto plan = DeviceTrustCoordinator::planNewDevice(device, false, true, true, {device},
                                                            db);
    QCOMPARE(plan.action, DeviceTrustCoordinator::NewDeviceAction::SkipDuplicatePrompt);
}

void TestDeviceTrustCoordinator::whitelistDriveAddsMissingPartitions()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo part1;
    part1.serial = QStringLiteral("S1");
    part1.vendor = QStringLiteral("V");
    part1.model = QStringLiteral("M");
    part1.parentDevice = QStringLiteral("/dev/sdf");
    part1.deviceNode = QStringLiteral("/dev/sdf1");
    DeviceInfo part2 = part1;
    part2.deviceNode = QStringLiteral("/dev/sdf2");

    const auto result = DeviceTrustCoordinator::whitelistDrivePartitions(
        QStringLiteral("/dev/sdf"), {part1, part2}, db, 1);
    QCOMPARE(result.recordsAdded, 2);
    QCOMPARE(result.whitelistedDeviceNodes.size(), 2);
    QVERIFY(db.hasDevice(part1));
    QVERIFY(db.hasDevice(part2));
}

QTEST_MAIN(TestDeviceTrustCoordinator)
#include "test_device_trust_coordinator.moc"
