#include <QtTest>

#include "DeviceVerificationPlanner.h"

using namespace FlashSpartan;

class TestDeviceVerificationPlanner : public QObject {
    Q_OBJECT

private slots:
    void fullPartitionStartsHash();
    void emptyMountPointRequestsMount();
    void mountedStartsManifest();
};

void TestDeviceVerificationPlanner::fullPartitionStartsHash()
{
    const auto plan = DeviceVerificationPlanner::planForDevice(VerificationProfile::FullPartition,
                                                             false, true);
    QCOMPARE(plan.action, DeviceVerificationPlanner::StartAction::StartFullPartitionHash);
}

void TestDeviceVerificationPlanner::emptyMountPointRequestsMount()
{
    const auto plan = DeviceVerificationPlanner::planForDevice(VerificationProfile::WatchManifest,
                                                             true, false);
    QCOMPARE(plan.action, DeviceVerificationPlanner::StartAction::MountThenVerify);
}

void TestDeviceVerificationPlanner::mountedStartsManifest()
{
    const auto plan = DeviceVerificationPlanner::planForDevice(VerificationProfile::Hybrid, false,
                                                             true);
    QCOMPARE(plan.action, DeviceVerificationPlanner::StartAction::StartManifestVerification);
}

QTEST_MAIN(TestDeviceVerificationPlanner)
#include "test_device_verification_planner.moc"
