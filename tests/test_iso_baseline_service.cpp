#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "IsoBaselineService.h"
#include "IsoQuickFingerprint.h"

using namespace FlashSpartan;

class TestIsoBaselineService : public QObject {
    Q_OBJECT

private slots:
    void relativePathFromMount();
    void storesAndComparesBaseline();
    void quickFingerprintDetectsTailChange();
    void baselineMismatchFailsPassed();
};

void TestIsoBaselineService::relativePathFromMount()
{
    QCOMPARE(IsoBaselineService::relativeImagePath(QStringLiteral("/mnt/usb"),
                                                   QStringLiteral("/mnt/usb/isos/debian.iso")),
             QStringLiteral("isos/debian.iso"));
}

void TestIsoBaselineService::storesAndComparesBaseline()
{
    IsoVerifyResult first;
    first.success = true;
    first.isoPath = QStringLiteral("/mnt/usb/debian.iso");
    first.computedSha256 = QStringLiteral("abc123");
    first.hashChecked = true;
    first.source = IsoVerifySource::ComputedOnly;

    const auto stored = IsoBaselineService::process(QStringLiteral("/mnt/usb"), {first}, {}, false,
                                                    true);
    QVERIFY(stored.baselinesChanged);
    QCOMPARE(stored.updatedBaselines.size(), 1);
    QCOMPARE(stored.updatedBaselines.first().relativePath, QStringLiteral("debian.iso"));

    IsoVerifyResult second = first;
    const auto compared =
        IsoBaselineService::process(QStringLiteral("/mnt/usb"), {second}, stored.updatedBaselines,
                                    true, false);
    QVERIFY(compared.results.first().baselineChecked);
    QVERIFY(compared.results.first().baselineMatches);
}

void TestIsoBaselineService::quickFingerprintDetectsTailChange()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.path() + QStringLiteral("/image.bin");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray payload(3 * 1024 * 1024, 'a');
    QVERIFY(file.write(payload) == payload.size());
    file.close();

    const QString original = IsoQuickFingerprint::compute(path);
    QVERIFY(!original.isEmpty());

    QVERIFY(file.open(QIODevice::ReadWrite));
    file.seek(file.size() - 1);
    char byte = 'b';
    QVERIFY(file.write(&byte, 1) == 1);
    file.close();

    const QString changed = IsoQuickFingerprint::compute(path);
    QVERIFY(!changed.isEmpty());
    QVERIFY(original != changed);
}

void TestIsoBaselineService::baselineMismatchFailsPassed()
{
    IsoVerifyResult result;
    result.success = true;
    result.isoPath = QStringLiteral("/mnt/usb/debian.iso");
    result.computedSha256 = QStringLiteral("bbbb");
    result.hashChecked = true;
    result.baselineChecked = true;
    result.baselineMatches = false;
    result.storedBaselineSha256 = QStringLiteral("aaaa");
    result.source = IsoVerifySource::ComputedOnly;
    QVERIFY(!result.passed());
    QVERIFY(!result.inconclusive());
}

QTEST_MAIN(TestIsoBaselineService)
#include "test_iso_baseline_service.moc"
