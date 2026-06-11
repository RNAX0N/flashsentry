#include <QtTest>

#include "MountOptionsUtil.h"

using namespace FlashSpartan;

class TestMountOptions : public QObject {
    Q_OBJECT

private slots:
    void defaultMountOptionsAreSecure();
    void readOnlyAddsRoOption();
    void forceUnmountSetsFlag();
};

void TestMountOptions::defaultMountOptionsAreSecure()
{
    MountOptions options;
    const QVariantMap map = MountOptionsUtil::toUdisksMountOptions(options);
    const QString opts = map.value(QStringLiteral("options")).toString();
    QVERIFY(opts.contains(QStringLiteral("noexec")));
    QVERIFY(opts.contains(QStringLiteral("nosuid")));
}

void TestMountOptions::readOnlyAddsRoOption()
{
    MountOptions options;
    options.readOnly = true;
    const QVariantMap map = MountOptionsUtil::toUdisksMountOptions(options);
    const QString opts = map.value(QStringLiteral("options")).toString();
    QVERIFY(opts.contains(QStringLiteral("ro")));
}

void TestMountOptions::forceUnmountSetsFlag()
{
    UnmountOptions options;
    options.force = true;
    const QVariantMap map = MountOptionsUtil::toUdisksUnmountOptions(options);
    QVERIFY(map.value(QStringLiteral("force")).toBool());
}

QTEST_MAIN(TestMountOptions)
#include "test_mount_options.moc"
