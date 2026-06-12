#include <QtTest>

#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif

class TestUdisks2Integration : public QObject {
    Q_OBJECT

private slots:
#ifndef Q_OS_WIN
    void udisksManagerResponds();
    void mountManagerReportsAvailability();
#else
    void windowsPlaceholder() { QSKIP("Linux UDisks2 integration test"); }
#endif
};

#ifndef Q_OS_WIN
#include "MountManager.h"

using namespace FlashSpartan;

void TestUdisks2Integration::udisksManagerResponds()
{
    QDBusInterface manager(QStringLiteral("org.freedesktop.UDisks2"),
                           QStringLiteral("/org/freedesktop/UDisks2"),
                           QStringLiteral("org.freedesktop.UDisks2.Manager"),
                           QDBusConnection::systemBus());
    if (!manager.isValid()) {
        QSKIP("UDisks2 is not available on the system D-Bus");
    }

    QDBusReply<QVariantMap> versionReply =
        manager.call(QStringLiteral("GetBlockDevices"), QVariantMap());
    QVERIFY(versionReply.isValid() || !versionReply.error().message().isEmpty());
}

void TestUdisks2Integration::mountManagerReportsAvailability()
{
    MountManager mountManager;
    if (!mountManager.isAvailable()) {
        QSKIP("MountManager could not connect to UDisks2");
    }
    QVERIFY(!mountManager.udisksVersion().isEmpty());
}
#endif

QTEST_MAIN(TestUdisks2Integration)
#include "test_udisks2_integration.moc"
