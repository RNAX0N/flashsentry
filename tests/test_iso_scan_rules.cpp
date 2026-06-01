#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "IsoScanRules.h"
#include "IsoVerifier.h"
#include "SettingsProfiles.h"

using namespace FlashSpartan;

class TestIsoScanRules : public QObject {
    Q_OBJECT

private slots:
    void reservedDirectories();
    void multibootLayoutDetection();
    void skipSmallEfiPartition();
    void profileIdMigration();
};

void TestIsoScanRules::reservedDirectories()
{
    QVERIFY(IsoScanRules::isReservedMultibootDirectory(QStringLiteral("ventoy")));
    QVERIFY(IsoScanRules::isReservedMultibootDirectory(QStringLiteral("EFI")));
    QVERIFY(!IsoScanRules::isReservedMultibootDirectory(QStringLiteral("isos")));
    QVERIFY(IsoScanRules::isExcludedImagePath(
        QStringLiteral("/mnt/usb/ventoy/bad.iso")));
    QVERIFY(!IsoScanRules::isExcludedImagePath(
        QStringLiteral("/mnt/usb/debian.iso")));
}

void TestIsoScanRules::multibootLayoutDetection()
{
#ifdef FLASHSPARTAN_TEST_FIXTURES_DIR
    const QString root = QStringLiteral(FLASHSPARTAN_TEST_FIXTURES_DIR) + QStringLiteral("/ventoy-scan");
#else
    QSKIP("fixtures path not defined");
#endif
    const MultibootLayout layout = IsoScanRules::detectMultibootLayout(root);
    QCOMPARE(layout.tool, MultibootTool::Ventoy);
    QVERIFY(layout.dataPartition);
    const QStringList files = IsoVerifier::findIsoFiles(root);
    QCOMPARE(files.size(), 3);
    for (const QString& path : files) {
        QVERIFY(!path.contains(QStringLiteral("/ventoy/")));
    }
}

void TestIsoScanRules::profileIdMigration()
{
    QCOMPARE(SettingsProfiles::normalizeProfileId(QStringLiteral("ventoy")),
             QStringLiteral("multi_image"));
    QCOMPARE(SettingsProfiles::normalizeProfileId(QStringLiteral("multi_image")),
             QStringLiteral("multi_image"));
    QCOMPARE(SettingsProfiles::profileDisplayName(QStringLiteral("ventoy")),
             QStringLiteral("Multi-image USB"));
}

void TestIsoScanRules::skipSmallEfiPartition()
{
    QTemporaryDir esp;
    QVERIFY(esp.isValid());
    QVERIFY(QDir(esp.path()).mkdir(QStringLiteral("EFI")));
    QVERIFY(IsoScanRules::shouldSkipAutoVerifyPartition(esp.path(), 32 * 1024 * 1024, 0));

    QTemporaryDir data;
    QVERIFY(data.isValid());
    QVERIFY(QDir(data.path()).mkdir(QStringLiteral("EFI")));
    QFile iso(data.path() + QStringLiteral("/debian.iso"));
    QVERIFY(iso.open(QIODevice::WriteOnly));
    iso.write("x");
    QVERIFY(!IsoScanRules::shouldSkipAutoVerifyPartition(data.path(), 32 * 1024 * 1024, 1));
}

QTEST_MAIN(TestIsoScanRules)
#include "test_iso_scan_rules.moc"
