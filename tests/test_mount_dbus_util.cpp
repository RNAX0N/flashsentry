#include <QtTest>

#include "MountDBusUtil.h"

using namespace FlashSpartan;

class TestMountDBusUtil : public QObject {
    Q_OBJECT

private slots:
    void mapsNotAuthorized();
    void passesThroughUnknown();
};

void TestMountDBusUtil::mapsNotAuthorized()
{
    const QString msg = MountDBusUtil::formatMountError(
        QStringLiteral("org.freedesktop.UDisks2.Error.NotAuthorized: no"));
    QVERIFY(msg.contains(QStringLiteral("Permission denied")));
}

void TestMountDBusUtil::passesThroughUnknown()
{
    const QString raw = QStringLiteral("custom backend failure");
    QCOMPARE(MountDBusUtil::formatMountError(raw), raw);
}

QTEST_MAIN(TestMountDBusUtil)
#include "test_mount_dbus_util.moc"
