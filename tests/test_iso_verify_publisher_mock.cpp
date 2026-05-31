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

using namespace FlashSentry;

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
#ifdef FLASHSENTRY_TEST_FIXTURES_DIR
    return QStringLiteral(FLASHSENTRY_TEST_FIXTURES_DIR);
#else
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../tests/fixtures"));
#endif
}

void TestIsoVerifyPublisherMock::initTestCase()
{
    QVERIFY(m_configHome.isValid());
    m_fixtureRoot = fixturesRoot() + QStringLiteral("/publisher-mock");

    qputenv("FLASHSENTRY_SKIP_REMOTE_CATALOG", "1");
    qputenv("FLASHSENTRY_SKIP_KEYSERVER_IMPORT", "1");
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", (m_configHome.path() + QStringLiteral("/cache")).toUtf8());

    QCoreApplication::setOrganizationName(QStringLiteral("FlashSentry"));
    QCoreApplication::setApplicationName(QStringLiteral("FlashSentryTest"));
    QStandardPaths::setTestModeEnabled(true);

    const QByteArray gpgHomeEnv = qgetenv("GNUPGHOME");
    if (!gpgHomeEnv.isEmpty() && !QDir(QString::fromUtf8(gpgHomeEnv)).exists()) {
        qunsetenv("GNUPGHOME");
    }
    qunsetenv("GNUPGHOME");

    if (qgetenv("FLASHSENTRY_GPG_PROGRAM").isEmpty() && FlashSentryTest::gpgAvailable()) {
        qputenv("FLASHSENTRY_GPG_PROGRAM", FlashSentryTest::gpgProgram().toUtf8());
    }
    if (!FlashSentryTest::gpgAvailable()) {
        QSKIP("gpg not available");
    }
    const QByteArray sourceRoot = qgetenv("FLASHSENTRY_SOURCE_ROOT");
    if (!sourceRoot.isEmpty()) {
        const QString sharedGpgHome =
            QDir::fromNativeSeparators(QString::fromUtf8(sourceRoot))
            + QStringLiteral("/.flashsentry-test-gpg-home");
        QDir().mkpath(sharedGpgHome);
        qputenv("FLASHSENTRY_TEST_GPG_HOME", QFile::encodeName(sharedGpgHome));
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

    const QString pubKey = fixturesRoot() + QStringLiteral("/catalog-signing/catalog-signing.pub");
    QVERIFY2(QFileInfo::exists(pubKey), qPrintable(pubKey));

    QString gpgHome = FlashSentry::gpgHomedirOverride();
    if (gpgHome.isEmpty()) {
        gpgHome = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                  + QStringLiteral("/iso-verify/gnupg");
    }
    QVERIFY(QDir().mkpath(gpgHome));
    QFile::setPermissions(gpgHome, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QProcess importProc;
    FlashSentry::configureGpgProcess(importProc);
    importProc.setArguments(FlashSentry::gpgBatchArgs()
                            << QStringLiteral("--homedir")
                            << QDir::toNativeSeparators(gpgHome)
                            << QStringLiteral("--import")
                            << QDir::toNativeSeparators(pubKey));
    importProc.start();
    QVERIFY(importProc.waitForFinished(30000));
    QVERIFY2(importProc.exitCode() == 0, qPrintable(importProc.readAllStandardError()));

    QProcess listProc;
    FlashSentry::configureGpgProcess(listProc);
    listProc.setArguments(FlashSentry::gpgBatchArgs()
                          << QStringLiteral("--homedir")
                          << QDir::toNativeSeparators(gpgHome)
                          << QStringLiteral("--list-keys")
                          << QStringLiteral("541DFAEB302C380671E666C7BBD811EF6FBA0EBC"));
    listProc.start();
    QVERIFY(listProc.waitForFinished(10000));
    QVERIFY2(listProc.exitCode() == 0, qPrintable(listProc.readAllStandardError()));
}

void TestIsoVerifyPublisherMock::cleanupTestCase()
{
    qunsetenv("FLASHSENTRY_SKIP_REMOTE_CATALOG");
    qunsetenv("FLASHSENTRY_SKIP_KEYSERVER_IMPORT");
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
    if (FlashSentryTest::skipGpgAssertionsOnWindowsCi()) {
        if (!r.pgpValid) {
            qWarning("Skipping publisher-mock PGP asserts on Windows CI: %s",
                     qPrintable(r.pgpSummary));
        }
        QVERIFY(r.passed() || (r.hashChecked && r.hashMatches));
    } else {
        QVERIFY2(r.pgpChecked, qPrintable(r.pgpSummary));
        QVERIFY2(r.pgpValid, qPrintable(r.pgpSummary));
        QVERIFY2(r.fingerprintTrusted, qPrintable(r.signingKeyFingerprint));
        QVERIFY(r.passed());
    }
    QCOMPARE(r.source, IsoVerifySource::LocalSidecar);
}

QTEST_MAIN(TestIsoVerifyPublisherMock)
#include "test_iso_verify_publisher_mock.moc"
