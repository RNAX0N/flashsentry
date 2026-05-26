#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "IsoCatalogManifest.h"
#include "IsoVerifier.h"

using namespace FlashSentry;

class TestIsoVerifyIntegration : public QObject {
    Q_OBJECT

    QTemporaryDir m_configHome;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void findIsoFilesVentoyLayout();
    void findChecksumSidecarPerFile();
    void findChecksumSidecarSumsFile();
    void offlineSidecarPass();
    void offlineSidecarMismatchFails();
    void userTofuHashPass();
};

static QString fixturesRoot()
{
#ifdef FLASHSENTRY_TEST_FIXTURES_DIR
    return QStringLiteral(FLASHSENTRY_TEST_FIXTURES_DIR);
#else
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../tests/fixtures"));
#endif
}

void TestIsoVerifyIntegration::initTestCase()
{
    QVERIFY(m_configHome.isValid());
    qputenv("FLASHSENTRY_SKIP_REMOTE_CATALOG", "1");
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", (m_configHome.path() + QStringLiteral("/cache")).toUtf8());

    QCoreApplication::setOrganizationName(QStringLiteral("FlashSentry"));
    QCoreApplication::setApplicationName(QStringLiteral("FlashSentry"));

    IsoVerifyOptions opt;
    opt.useHashCache = false;
    opt.preferOfflineSidecars = true;
    opt.maxParallel = 1;
    IsoVerifier::setVerifyOptions(opt);
    IsoCatalogManifest::ensureLoaded();
}

void TestIsoVerifyIntegration::cleanupTestCase()
{
    qunsetenv("FLASHSENTRY_SKIP_REMOTE_CATALOG");
}

void TestIsoVerifyIntegration::findIsoFilesVentoyLayout()
{
    const QString root = fixturesRoot() + QStringLiteral("/ventoy-scan");
    const QStringList files = IsoVerifier::findIsoFiles(root);
    QCOMPARE(files.size(), 3);
    QVERIFY(files.at(0).endsWith(QStringLiteral("debian-12.5.0-amd64-netinst.iso")));
    QVERIFY(files.at(1).endsWith(QStringLiteral("nested/raspios-2024.img.xz")));
    QVERIFY(files.at(2).endsWith(QStringLiteral("ubuntu-24.04.2-desktop-amd64.iso")));
}

void TestIsoVerifyIntegration::findChecksumSidecarPerFile()
{
    const QString iso = fixturesRoot() + QStringLiteral("/offline-good/zz-offline-fixture.iso");
    const QString sidecar = IsoVerifier::findChecksumSidecar(iso);
    QVERIFY(sidecar.endsWith(QStringLiteral("zz-offline-fixture.iso.sha256")));
}

void TestIsoVerifyIntegration::findChecksumSidecarSumsFile()
{
    const QString iso = fixturesRoot() + QStringLiteral("/offline-sums-only/zz-offline-fixture.iso");
    const QString sidecar = IsoVerifier::findChecksumSidecar(iso);
    QVERIFY(sidecar.endsWith(QStringLiteral("/SHA256SUMS")));
}

void TestIsoVerifyIntegration::offlineSidecarPass()
{
    const QString iso = fixturesRoot() + QStringLiteral("/offline-good/zz-offline-fixture.iso");
    const IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(iso);
    QVERIFY(r.success);
    QVERIFY(r.hashChecked);
    QVERIFY(r.hashMatches);
    QVERIFY(r.passed());
    QCOMPARE(r.source, IsoVerifySource::LocalSidecar);
}

void TestIsoVerifyIntegration::offlineSidecarMismatchFails()
{
    const QString iso = fixturesRoot() + QStringLiteral("/offline-bad/zz-offline-fixture.iso");
    const IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(iso);
    QVERIFY(r.hashChecked);
    QVERIFY(!r.hashMatches);
    QVERIFY(!r.passed());
}

void TestIsoVerifyIntegration::userTofuHashPass()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString isoPath = dir.filePath(QStringLiteral("my-custom-build.iso"));
    QFile iso(isoPath);
    QVERIFY(iso.open(QIODevice::WriteOnly));
    iso.write("flashsentry-tofu");
    iso.close();

    const QString hash =
        QStringLiteral("8fb83f7d50c8e4e41ea2e67fcbd5a94e9284c88837b4a7dacb93d64f28af43aa");
    QVERIFY(IsoCatalogManifest::trustUserHash(QStringLiteral("my-custom-build.iso"), hash));

    IsoCatalogManifest::reload();
    const IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(isoPath);
    QVERIFY(r.hashMatches);
    QVERIFY(r.passed());
    QCOMPARE(r.source, IsoVerifySource::EmbeddedCatalog);
}

QTEST_MAIN(TestIsoVerifyIntegration)
#include "test_iso_verify_integration.moc"
