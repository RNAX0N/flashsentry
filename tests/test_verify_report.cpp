#include <QtTest>
#include "IsoVerifyReport.h"

using namespace FlashSentry;

class TestVerifyReport : public QObject {
    Q_OBJECT

private slots:
    void summaryCountsSidecarNeeded();
    void csvContainsHeader();
};

void TestVerifyReport::summaryCountsSidecarNeeded()
{
    IsoVerifyResult fail;
    fail.success = true;
    fail.hashChecked = true;
    fail.hashMatches = true;
    fail.expectedSha256 = QStringLiteral("deadbeef");
    fail.isoPath = QStringLiteral("/x.iso");

    IsoVerifyResult needs;
    needs.success = true;
    needs.hashChecked = true;
    needs.isoPath = QStringLiteral("/y.iso");

    const QString s = IsoVerifyReport::summaryLine({fail, needs});
    QVERIFY(s.contains(QStringLiteral("1/2")));
    QVERIFY(s.contains(QStringLiteral("sidecar")));
}

void TestVerifyReport::csvContainsHeader()
{
    const QString csv = IsoVerifyReport::buildCsv({});
    QVERIFY(csv.startsWith(QStringLiteral("file,publisher")));
}

QTEST_MAIN(TestVerifyReport)
#include "test_verify_report.moc"
