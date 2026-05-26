#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "ManifestService.h"

using namespace FlashSentry;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

} // namespace

class TestManifestService : public QObject {
    Q_OBJECT

private slots:
    void buildsManifestFromRelativePaths();
    void rejectsRelativeTraversalOutsideMount();
    void rejectsAbsolutePathOutsideMount();
    void rejectsSymlinkEscape();
    void manifestRootMismatchFailsVerification();
};

void TestManifestService::buildsManifestFromRelativePaths()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString mountPoint = tempDir.path() + QStringLiteral("/mount");
    QVERIFY(QDir().mkpath(mountPoint + QStringLiteral("/docs")));
    writeFile(mountPoint + QStringLiteral("/docs/file.txt"), "trusted contents");

    WatchGroup group;
    group.id = QStringLiteral("docs");
    group.name = QStringLiteral("Documents");
    group.watchPaths = {QStringLiteral("docs")};

    const auto result = ManifestService::buildGroup(mountPoint, group);
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.group.files.size(), 1);
    QCOMPARE(result.group.files.first().relativePath, QStringLiteral("docs/file.txt"));
    QVERIFY(!result.group.merkleRoot.isEmpty());
}

void TestManifestService::rejectsRelativeTraversalOutsideMount()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString mountPoint = tempDir.path() + QStringLiteral("/mount");
    QVERIFY(QDir().mkpath(mountPoint));
    writeFile(tempDir.path() + QStringLiteral("/outside.txt"), "host file");

    WatchGroup group;
    group.id = QStringLiteral("escape");
    group.name = QStringLiteral("Escape");
    group.watchPaths = {QStringLiteral("../outside.txt")};

    const auto result = ManifestService::buildGroup(mountPoint, group);
    QVERIFY(!result.success);
    QVERIFY2(result.errorMessage.contains(QStringLiteral("escapes mount point")),
             qPrintable(result.errorMessage));
}

void TestManifestService::rejectsAbsolutePathOutsideMount()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString mountPoint = tempDir.path() + QStringLiteral("/mount");
    QVERIFY(QDir().mkpath(mountPoint));
    const QString outsidePath = tempDir.path() + QStringLiteral("/outside.txt");
    writeFile(outsidePath, "host file");

    WatchGroup group;
    group.id = QStringLiteral("absolute");
    group.name = QStringLiteral("Absolute");
    group.watchPaths = {outsidePath};

    const auto result = ManifestService::buildGroup(mountPoint, group);
    QVERIFY(!result.success);
    QVERIFY2(result.errorMessage.contains(QStringLiteral("escapes mount point")),
             qPrintable(result.errorMessage));
}

void TestManifestService::rejectsSymlinkEscape()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString mountPoint = tempDir.path() + QStringLiteral("/mount");
    QVERIFY(QDir().mkpath(mountPoint));
    const QString outsidePath = tempDir.path() + QStringLiteral("/outside.txt");
    writeFile(outsidePath, "host file");
    QVERIFY(QFile::link(outsidePath, mountPoint + QStringLiteral("/outside-link.txt")));

    WatchGroup group;
    group.id = QStringLiteral("symlink");
    group.name = QStringLiteral("Symlink");
    group.watchPaths = {QStringLiteral("outside-link.txt")};

    const auto result = ManifestService::buildGroup(mountPoint, group);
    QVERIFY(!result.success);
    QVERIFY2(result.errorMessage.contains(QStringLiteral("escapes mount point")),
             qPrintable(result.errorMessage));
}

void TestManifestService::manifestRootMismatchFailsVerification()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString mountPoint = tempDir.path() + QStringLiteral("/mount");
    QVERIFY(QDir().mkpath(mountPoint + QStringLiteral("/docs")));
    writeFile(mountPoint + QStringLiteral("/docs/file.txt"), "trusted contents");

    WatchGroup group;
    group.id = QStringLiteral("docs");
    group.name = QStringLiteral("Documents");
    group.watchPaths = {QStringLiteral("docs")};
    auto built = ManifestService::buildGroup(mountPoint, group);
    QVERIFY2(built.success, qPrintable(built.errorMessage));

    WatchManifest manifest;
    manifest.groups.append(built.group);
    manifest.manifestRoot = QStringLiteral("0");

    const auto result = ManifestService::verifyManifest(mountPoint, manifest);
    QVERIFY(result.success);
    QVERIFY(!result.matches);
}

QTEST_MAIN(TestManifestService)
#include "test_manifest_service.moc"
