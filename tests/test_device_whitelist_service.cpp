#include <QtTest>
#include <QTemporaryDir>
#include <QProcessEnvironment>

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

class TestDeviceWhitelistService : public QObject {
    Q_OBJECT

private slots:
    void makeRecordUsesCanonicalId();
    void weakIdentityNoticeOnlyWhenSerialMissing();
};

void TestDeviceWhitelistService::makeRecordUsesCanonicalId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    installIsolatedPolicy(tempDir.path());

    DatabaseManager db;
    QVERIFY(db.initialize());

    DeviceInfo info;
    info.serial = QStringLiteral("SER100");
    info.vendor = QStringLiteral("Vendor");
    info.model = QStringLiteral("Model");
    info.deviceNode = QStringLiteral("/dev/sdz9");

    const DeviceRecord record = DeviceWhitelistService::makeRecord(info, db, 1);
    QCOMPARE(record.uniqueId, info.partitionUniqueId());
    QCOMPARE(record.trustLevel, 1);
}

void TestDeviceWhitelistService::weakIdentityNoticeOnlyWhenSerialMissing()
{
    DeviceInfo weak;
    weak.vendor = QStringLiteral("Generic");
    weak.model = QStringLiteral("Flash");
    weak.deviceNode = QStringLiteral("/dev/sdb1");
    QVERIFY(weak.hasWeakIdentity());
    QVERIFY(!DeviceWhitelistService::weakIdentityNoticeHtml(weak).isEmpty());

    DeviceInfo strong = weak;
    strong.serial = QStringLiteral("ABC");
    QVERIFY(!strong.hasWeakIdentity());
    QVERIFY(DeviceWhitelistService::weakIdentityNoticeHtml(strong).isEmpty());
}

QTEST_MAIN(TestDeviceWhitelistService)
#include "test_device_whitelist_service.moc"
