#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "IsoVerifyReport.h"

using namespace FlashSpartan;

class TestVerifyReport : public QObject {
    Q_OBJECT

private slots:
    void summaryCountsSidecarNeeded();
    void csvContainsHeader();
    void countSummaryMatchesSummaryLine();
    void jsonContainsSummaryAndResults();
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
    needs.source = IsoVerifySource::ComputedOnly;

    const QString s = IsoVerifyReport::summaryLine({fail, needs});
    QVERIFY(s.contains(QStringLiteral("1/2")));
    QVERIFY(s.contains(QStringLiteral("sidecar")));
}

void TestVerifyReport::csvContainsHeader()
{
    const QString csv = IsoVerifyReport::buildCsv({});
    QVERIFY(csv.startsWith(QStringLiteral("file,publisher")));
}

void TestVerifyReport::countSummaryMatchesSummaryLine()
{
    IsoVerifyResult ok;
    ok.success = true;
    ok.hashChecked = true;
    ok.hashMatches = true;
    ok.expectedSha256 = QStringLiteral("ab");
    ok.isoPath = QStringLiteral("/a.iso");

    const QList<IsoVerifyResult> results = {ok};
    const auto counts = IsoVerifyReport::countSummary(results);
    QCOMPARE(counts.passed, 1);
    QCOMPARE(counts.total, 1);
    QCOMPARE(counts.needsSidecar, 0);
    QVERIFY(IsoVerifyReport::summaryLine(results).contains(QStringLiteral("1/1")));
}

void TestVerifyReport::jsonContainsSummaryAndResults()
{
    IsoVerifyResult r;
    r.success = true;
    r.hashChecked = true;
    r.hashMatches = true;
    r.expectedSha256 = QStringLiteral("ab");
    r.isoPath = QStringLiteral("/tmp/test.iso");
    r.publisherName = QStringLiteral("Debian");

    const QJsonDocument doc =
        QJsonDocument::fromJson(IsoVerifyReport::buildJson({r}).toUtf8());
    QVERIFY(doc.isObject());
    const QJsonObject root = doc.object();
    QVERIFY(root.contains(QStringLiteral("summary")));
    QVERIFY(root.contains(QStringLiteral("results")));
    const QJsonArray items = root.value(QStringLiteral("results")).toArray();
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.at(0).toObject().value(QStringLiteral("file")).toString(),
             QStringLiteral("test.iso"));
}

QTEST_MAIN(TestVerifyReport)
#include "test_verify_report.moc"
