#include <QtTest>

#include "HashWorker.h"

using namespace FlashSpartan;

class TestHashWorker : public QObject {
    Q_OBJECT

private slots:
    void cancelUnknownJobReturnsFalse();
    void cancelAllOnIdleWorker();
    void algorithmNameRoundTrip();
};

void TestHashWorker::cancelUnknownJobReturnsFalse()
{
    HashWorker worker;
    QVERIFY(!worker.cancelHash(QStringLiteral("missing-job")));
    QVERIFY(!worker.hasActiveJobs());
}

void TestHashWorker::cancelAllOnIdleWorker()
{
    HashWorker worker;
    worker.cancelAll();
    QCOMPARE(worker.activeJobCount(), 0);
}

void TestHashWorker::algorithmNameRoundTrip()
{
    QCOMPARE(HashWorker::algorithmFromName(QStringLiteral("SHA256")),
             HashWorker::Algorithm::SHA256);
    QCOMPARE(HashWorker::algorithmName(HashWorker::Algorithm::BLAKE2b),
             QStringLiteral("BLAKE2b"));
}

QTEST_MAIN(TestHashWorker)
#include "test_hash_worker.moc"
