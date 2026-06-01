#include <QtTest>
#include <QTemporaryDir>

#include "BadUsbBaselineStore.h"

using namespace FlashSpartan;

class TestBadUsbBaselineStore : public QObject {
    Q_OBJECT

private slots:
    void roundTripTrustedDevice();
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
    QVERIFY(store.upsertBaseline(info, true, QStringLiteral("test")));
    QVERIFY(store.hasDevice(info.stableId()));

    BadUsbBaselineStore loaded;
    QVERIFY(loaded.initialize(path));
    const auto entry = loaded.getDevice(info.stableId());
    QVERIFY(entry.has_value());
    QVERIFY(entry->trusted);
    QCOMPARE(entry->device.product, QStringLiteral("Receiver"));
    QVERIFY(entry->device.capabilities.contains(QStringLiteral("keyboard")));
}

QTEST_MAIN(TestBadUsbBaselineStore)
#include "test_badusb_baseline.moc"
