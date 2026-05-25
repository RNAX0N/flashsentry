#include <QtTest>
#include "IsoChecksum.h"

using namespace FlashSentry;

class TestIsoChecksum : public QObject {
    Q_OBJECT

private slots:
    void sumsFileWithAsteriskPrefix();
    void singleLineHex();
    void missingIsoNameFails();
    void rockyChecksumFormat();
};

void TestIsoChecksum::sumsFileWithAsteriskPrefix()
{
    const QString content = QStringLiteral(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  *ubuntu-24.04.iso\n"
        "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef  other.iso\n");
    const QString hash = IsoChecksum::parseSha256Content(content, QStringLiteral("ubuntu-24.04.iso"));
    QCOMPARE(hash, QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

void TestIsoChecksum::singleLineHex()
{
    const QString content = QStringLiteral("a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3\n");
    const QString hash = IsoChecksum::parseSha256Content(content, QStringLiteral("ignored.iso"));
    QCOMPARE(hash, QStringLiteral("a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3"));
}

void TestIsoChecksum::missingIsoNameFails()
{
    QString err;
    const QString hash = IsoChecksum::parseSha256Content(
        QStringLiteral("deadbeef  other.iso\n"), QStringLiteral("missing.iso"), &err);
    QVERIFY(hash.isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestIsoChecksum::rockyChecksumFormat()
{
    const QString content = QStringLiteral(
        "SHA256 (Rocky-9.4-x86_64-minimal.iso) = "
        "1e5d7da3d84d5d9a5a1177858a5df21b868390bfccf7f0f419b1e59acc293160\n");
    const QString hash = IsoChecksum::parseSha256Content(content,
                                                         QStringLiteral("Rocky-9.4-x86_64-minimal.iso"));
    QCOMPARE(hash, QStringLiteral("1e5d7da3d84d5d9a5a1177858a5df21b868390bfccf7f0f419b1e59acc293160"));
}

QTEST_MAIN(TestIsoChecksum)
#include "test_iso_checksum.moc"
