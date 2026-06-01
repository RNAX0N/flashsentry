#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

#include "GpgTestUtil.h"
#include "WindowsCiTestUtil.h"
#include "IsoCatalogManifest.h"
#include "IsoVerifier.h"

using namespace FlashSpartan;

class TestIsoVerifyPublisherMock : public QObject {
    Q_OBJECT

    QTemporaryDir m_configHome;
    QString m_fixtureRoot;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void mockPublisherFullChainPass();
};

static QString fixturesRoot()
{
#ifdef FLASHSPARTAN_TEST_FIXTURES_DIR
    return QStringLiteral(FLASHSPARTAN_TEST_FIXTURES_DIR);
#else
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../tests/fixtures"));
#endif
}

void TestIsoVerifyPublisherMock::initTestCase()
{
    QVERIFY(m_configHome.isValid());
    m_fixtureRoot = fixturesRoot() + QStringLiteral("/publisher-mock");

    qputenv("FLASHSPARTAN_SKIP_REMOTE_CATALOG", "1");
    qputenv("FLASHSPARTAN_SKIP_KEYSERVER_IMPORT", "1");
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", (m_configHome.path() + QStringLiteral("/cache")).toUtf8());

    QCoreApplication::setOrganizationName(QStringLiteral("FlashSpartan"));
    QCoreApplication::setApplicationName(QStringLiteral("FlashSpartanTest"));
    QStandardPaths::setTestModeEnabled(true);

    const QByteArray gpgHomeEnv = qgetenv("GNUPGHOME");
    if (!gpgHomeEnv.isEmpty() && !QDir(QString::fromUtf8(gpgHomeEnv)).exists()) {
        qunsetenv("GNUPGHOME");
    }
    qunsetenv("GNUPGHOME");

    if (qgetenv("FLASHSPARTAN_GPG_PROGRAM").isEmpty() && FlashSpartanTest::gpgAvailable()) {
        qputenv("FLASHSPARTAN_GPG_PROGRAM", FlashSpartanTest::gpgProgram().toUtf8());
    }
    if (!FlashSpartanTest::gpgAvailable()) {
        QSKIP("gpg not available");
    }
    const QByteArray sourceRoot = qgetenv("FLASHSPARTAN_SOURCE_ROOT");
    if (!sourceRoot.isEmpty()) {
        const QString sharedGpgHome =
            QDir::fromNativeSeparators(QString::fromUtf8(sourceRoot))
            + QStringLiteral("/.flashspartan-test-gpg-home");
        QDir().mkpath(sharedGpgHome);
        qputenv("FLASHSPARTAN_TEST_GPG_HOME", QFile::encodeName(sharedGpgHome));
    }
    const QString dropInDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                              + QStringLiteral("/iso-catalog.d");
    QDir().mkpath(dropInDir);
    const QString dropInPath = dropInDir + QStringLiteral("/mock-publisher.json");
    if (QFile::exists(dropInPath)) {
        QVERIFY(QFile::remove(dropInPath));
    }
    QVERIFY(QFile::copy(m_fixtureRoot + QStringLiteral("/catalog-dropin.json"), dropInPath));

    IsoVerifyOptions opt;
    opt.useHashCache = false;
    opt.preferOfflineSidecars = true;
    opt.maxParallel = 1;
    IsoVerifier::setVerifyOptions(opt);

    IsoCatalogManifest::reload();

    if (FlashSpartanTest::skipGpgAssertionsOnWindowsCi()) {
        return;
    }

    const QString pubKey = fixturesRoot() + QStringLiteral("/catalog-signing/catalog-signing.pub");
    QVERIFY2(QFileInfo::exists(pubKey), qPrintable(pubKey));

    QString gpgHome = FlashSpartan::gpgHomedirOverride();
    if (gpgHome.isEmpty()) {
        gpgHome = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                  + QStringLiteral("/iso-verify/gnupg");
    }
    QVERIFY(QDir().mkpath(gpgHome));
    QFile::setPermissions(gpgHome, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QProcess importProc;
    FlashSpartan::configureGpgProcess(importProc);
    importProc.setArguments(FlashSpartan::gpgBatchArgs()
                            << QStringLiteral("--homedir")
                            << QDir::toNativeSeparators(gpgHome)
                            << QStringLiteral("--import")
                            << QDir::toNativeSeparators(pubKey));
    importProc.setProcessChannelMode(QProcess::MergedChannels);
    importProc.start();
    QVERIFY(importProc.waitForFinished(30000));
    QVERIFY2(importProc.exitCode() == 0, qPrintable(QString::fromUtf8(importProc.readAll())));

    QProcess listProc;
    FlashSpartan::configureGpgProcess(listProc);
    listProc.setArguments(FlashSpartan::gpgBatchArgs()
                          << QStringLiteral("--homedir")
                          << QDir::toNativeSeparators(gpgHome)
                          << QStringLiteral("--list-keys")
                          << QStringLiteral("541DFAEB302C380671E666C7BBD811EF6FBA0EBC"));
    listProc.setProcessChannelMode(QProcess::MergedChannels);
    listProc.start();
    QVERIFY(listProc.waitForFinished(10000));
    QVERIFY2(listProc.exitCode() == 0, qPrintable(QString::fromUtf8(listProc.readAll())));
}

void TestIsoVerifyPublisherMock::cleanupTestCase()
{
    qunsetenv("FLASHSPARTAN_SKIP_REMOTE_CATALOG");
    qunsetenv("FLASHSPARTAN_SKIP_KEYSERVER_IMPORT");
}

void TestIsoVerifyPublisherMock::mockPublisherFullChainPass()
{
    const auto catalogMatch =
        IsoCatalogManifest::lookup(QStringLiteral("mock-distro-1.0.0-x86_64.iso"));
    QVERIFY(catalogMatch.has_value());
    QCOMPARE(catalogMatch->publisherId, QStringLiteral("mock-publisher"));

    const QString iso = m_fixtureRoot + QStringLiteral("/mock-distro-1.0.0-x86_64.iso");
    QVERIFY(!IsoVerifier::findChecksumSidecar(iso).isEmpty());
    QVERIFY(!IsoVerifier::findSignatureSidecar(iso).isEmpty());

    const IsoVerifyResult r = IsoVerifier::verifyIsoAutomated(iso);
    QVERIFY2(r.success, qPrintable(r.errorMessage));
    QVERIFY(r.hashChecked);
    QVERIFY2(r.hashMatches, qPrintable(r.reportSummary));
    QCOMPARE(r.source, IsoVerifySource::LocalSidecar);

    if (FlashSpartanTest::skipGpgAssertionsOnWindowsCi()) {
        if (!r.pgpValid) {
            qWarning("Skipping publisher-mock PGP asserts on Windows CI: %s",
                     qPrintable(r.pgpSummary));
        }
        return;
    }

    QVERIFY2(r.pgpChecked, qPrintable(r.pgpSummary));
    QVERIFY2(r.pgpValid, qPrintable(r.pgpSummary));
    QVERIFY2(r.fingerprintTrusted, qPrintable(r.signingKeyFingerprint));
    QVERIFY(r.passed());
}

QTEST_MAIN(TestIsoVerifyPublisherMock)
#include "test_iso_verify_publisher_mock.moc"
