#include <QtTest>

#include "IsoBaselineService.h"
#include "IsoVerifyReport.h"
#include "IsoVerifier.h"

using namespace FlashSpartan;

/**
 * Exercises the post-verify coordinator path used by MainWindow::handleIsoVerificationReport
 * without spinning up the full GUI.
 */
class TestMainWindowIsoFlow : public QObject {
    Q_OBJECT

private slots:
    void historyStatusPassWhenAllVerified();
    void historyStatusPartialWhenOnlyInconclusive();
    void historyStatusFailWhenBaselineMismatch();
    void mountScanFailureOnBaselineMismatch();
};

void TestMainWindowIsoFlow::historyStatusPassWhenAllVerified()
{
    IsoVerifyResult ok;
    ok.success = true;
    ok.isoPath = QStringLiteral("/mnt/usb/debian.iso");
    ok.hashChecked = true;
    ok.hashMatches = true;
    ok.expectedSha256 = QStringLiteral("aa");
    const auto counts = IsoVerifyReport::countSummary({ok});
    QCOMPARE(counts.passed, 1);
    QCOMPARE(IsoVerifyReport::countFailed(counts), 0);
}

void TestMainWindowIsoFlow::historyStatusPartialWhenOnlyInconclusive()
{
    IsoVerifyResult inconclusive;
    inconclusive.success = true;
    inconclusive.isoPath = QStringLiteral("/mnt/usb/custom.iso");
    inconclusive.hashChecked = true;
    inconclusive.computedSha256 = QStringLiteral("aa");
    inconclusive.source = IsoVerifySource::ComputedOnly;
    const auto counts = IsoVerifyReport::countSummary({inconclusive});
    QCOMPARE(counts.needsSidecar, 1);
    QVERIFY(!IsoVerifier::mountScanHasFailures({inconclusive}));
}

void TestMainWindowIsoFlow::historyStatusFailWhenBaselineMismatch()
{
    IsoVerifyResult changed;
    changed.success = true;
    changed.isoPath = QStringLiteral("/mnt/usb/debian.iso");
    changed.hashChecked = true;
    changed.computedSha256 = QStringLiteral("bbbb");
    changed.baselineChecked = true;
    changed.baselineMatches = false;
    changed.storedBaselineSha256 = QStringLiteral("aaaa");
    QVERIFY(!changed.passed());
    QVERIFY(IsoVerifier::mountScanHasFailures({changed}));
}

void TestMainWindowIsoFlow::mountScanFailureOnBaselineMismatch()
{
    IsoVerifyResult first;
    first.success = true;
    first.isoPath = QStringLiteral("/mnt/usb/debian.iso");
    first.computedSha256 = QStringLiteral("1111");
    first.hashChecked = true;

    const auto stored = IsoBaselineService::process(QStringLiteral("/mnt/usb"), {first}, {}, false,
                                                    true);
  IsoVerifyResult second;
    second.success = true;
    second.isoPath = QStringLiteral("/mnt/usb/debian.iso");
    second.computedSha256 = QStringLiteral("2222");
    second.hashChecked = true;
    const auto compared =
        IsoBaselineService::process(QStringLiteral("/mnt/usb"), {second}, stored.updatedBaselines,
                                    true, false);
    QVERIFY(IsoVerifier::mountScanHasFailures(compared.results));
}

QTEST_MAIN(TestMainWindowIsoFlow)
#include "test_main_window_iso_flow.moc"
