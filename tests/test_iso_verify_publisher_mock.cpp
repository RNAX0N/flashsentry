#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

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
#ifdef Q_OS_WIN
    QSKIP("Publisher OpenPGP integration is skipped on Windows until Gpg4win support is validated");
#endif
    if (QStandardPaths::findExecutable(QStringLiteral("gpg")).isEmpty()) {
        QSKIP("gpg is required for publisher signature-chain integration test");
    }

    QVERIFY(m_configHome.isValid());
    m_fixtureRoot = fixturesRoot() + QStringLiteral("/publisher-mock");

    qputenv("FLASHSENTRY_SKIP_REMOTE_CATALOG", "1");
    qputenv("FLASHSENTRY_SKIP_KEYSERVER_IMPORT", "1");
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", (m_configHome.path() + QStringLiteral("/cache")).toUtf8());

    QCoreApplication::setOrganizationName(QStringLiteral("FlashSentry"));
    QCoreApplication::setApplicationName(QStringLiteral("FlashSentry"));

    const QString dropInDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                              + QStringLiteral("/iso-catalog.d");
    QDir().mkpath(dropInDir);
    QVERIFY(QFile::copy(m_fixtureRoot + QStringLiteral("/catalog-dropin.json"),
                          dropInDir + QStringLiteral("/mock-publisher.json")));

    IsoVerifyOptions opt;
    opt.useHashCache = false;
    opt.preferOfflineSidecars = true;
    opt.maxParallel = 1;
    IsoVerifier::setVerifyOptions(opt);

    IsoCatalogManifest::reload();

    const QString pubKey = fixturesRoot() + QStringLiteral("/catalog-signing/catalog-signing.pub");
    QVERIFY2(QFileInfo::exists(pubKey), qPrintable(pubKey));

    const QString gpgHome =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/iso-verify/gnupg");
    QVERIFY(QDir().mkpath(gpgHome));
    QFile::setPermissions(gpgHome, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    QProcess importProc;
    importProc.setProgram(QStringLiteral("gpg"));
    importProc.setArguments({QStringLiteral("--homedir"),
                             gpgHome,
                             QStringLiteral("--batch"),
                             QStringLiteral("--import"),
                             pubKey});
    importProc.start();
    QVERIFY(importProc.waitForFinished(30000));

    QProcess listProc;
    listProc.setProgram(QStringLiteral("gpg"));
    listProc.setArguments({QStringLiteral("--homedir"),
                           gpgHome,
                           QStringLiteral("--list-keys"),
                           QStringLiteral("541DFAEB302C380671E666C7BBD811EF6FBA0EBC")});
    listProc.start();
    QVERIFY(listProc.waitForFinished(10000));
    QVERIFY2(listProc.exitCode() == 0, qPrintable(importProc.readAllStandardError()));
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
    QVERIFY2(r.pgpChecked, qPrintable(r.pgpSummary));
    QVERIFY2(r.pgpValid, qPrintable(r.pgpSummary));
    QVERIFY2(r.fingerprintTrusted, qPrintable(r.signingKeyFingerprint));
    QVERIFY(r.passed());
    QCOMPARE(r.source, IsoVerifySource::LocalSidecar);
}

QTEST_MAIN(TestIsoVerifyPublisherMock)
#include "test_iso_verify_publisher_mock.moc"
