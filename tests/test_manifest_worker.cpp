#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "ManifestWorker.h"
#include "ManifestService.h"

using namespace FlashSpartan;

class TestManifestWorker : public QObject {
    Q_OBJECT

private slots:
    void cancelUnknownJobReturnsFalse();
    void cancelledVerifyDoesNotEmitCompleted();
};

void TestManifestWorker::cancelUnknownJobReturnsFalse()
{
    ManifestWorker worker;
    QVERIFY(!worker.cancelJob(QStringLiteral("missing-job")));
}

void TestManifestWorker::cancelledVerifyDoesNotEmitCompleted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile file(dir.filePath(QStringLiteral("note.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("manifest-worker-cancel-test");
    file.close();

    WatchGroup group;
    group.id = QStringLiteral("docs");
    group.name = QStringLiteral("Docs");
    group.watchPaths = {QStringLiteral(".")};

    WatchManifest manifest;
    manifest.groups = {group};
    manifest.manifestRoot = QStringLiteral("placeholder");

    ManifestWorker worker;
    QSignalSpy completedSpy(&worker, &ManifestWorker::manifestCompleted);
    QSignalSpy failedSpy(&worker, &ManifestWorker::manifestFailed);

    const QString jobId =
        worker.startVerify(QStringLiteral("/dev/null"), dir.path(), QStringLiteral("test-device"),
                           manifest);
    QVERIFY(!jobId.isEmpty());
    QVERIFY(worker.cancelJob(jobId));

    QTRY_COMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 0);
}

QTEST_MAIN(TestManifestWorker)
#include "test_manifest_worker.moc"
