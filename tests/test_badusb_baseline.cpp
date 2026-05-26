#include <QtTest>
#include <QTemporaryDir>

#include "BadUsbBaselineStore.h"

using namespace FlashSentry;

class TestBadUsbBaselineStore : public QObject {
    Q_OBJECT

private slots:
    void roundTripTrustedDevice();
    void matchesBaselineByIdentifierAlias();
};

void TestBadUsbBaselineStore::roundTripTrustedDevice()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("badusb-baseline.json"));

    HidDeviceInfo info;
    info.vendorId = QStringLiteral("046d");
    info.productId = QStringLiteral("c52b");
    info.serial = QStringLiteral("SERIAL");
    info.product = QStringLiteral("Receiver");
    info.capabilities = {QStringLiteral("keyboard"), QStringLiteral("mouse")};

    BadUsbBaselineStore store;
    QVERIFY(store.initialize(path));
    QVERIFY(store.upsertBaseline(info, true, QStringLiteral("test"), HidDeviceCategory::Receiver));
    QVERIFY(store.hasDevice(info.stableId()));

    BadUsbBaselineStore loaded;
    QVERIFY(loaded.initialize(path));
    const auto entry = loaded.getDevice(info.stableId());
    QVERIFY(entry.has_value());
    QVERIFY(entry->trusted);
    QCOMPARE(entry->userCategory, HidDeviceCategory::Receiver);
    QCOMPARE(entry->device.product, QStringLiteral("Receiver"));
    QVERIFY(entry->device.capabilities.contains(QStringLiteral("keyboard")));
}

void TestBadUsbBaselineStore::matchesBaselineByIdentifierAlias()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    HidDeviceInfo first;
    first.vendorId = QStringLiteral("046d");
    first.productId = QStringLiteral("c31c");
    first.serial = QStringLiteral("SERIAL");
    first.usbPath = QStringLiteral("/sys/bus/usb/devices/1-1");
    first.product = QStringLiteral("Keyboard");
    first.capabilities = {QStringLiteral("keyboard")};

    HidDeviceInfo moved = first;
    moved.usbPath = QStringLiteral("/sys/bus/usb/devices/1-2");

    BadUsbBaselineStore store;
    QVERIFY(store.initialize(dir.filePath(QStringLiteral("badusb-baseline.json"))));
    QVERIFY(store.upsertBaseline(first, true, QString(), HidDeviceCategory::Keyboard));

    const auto match = store.matchDevice(moved);
    QVERIFY(match.has_value());
    QCOMPARE(match->userCategory, HidDeviceCategory::Keyboard);
    QVERIFY(match->identifierAliases.contains(first.vidPidSerialKey()));
}

QTEST_MAIN(TestBadUsbBaselineStore)
#include "test_badusb_baseline.moc"
