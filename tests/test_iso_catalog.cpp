#include <QtTest>
#include "IsoCatalog.h"

using namespace FlashSentry;

class TestIsoCatalog : public QObject {
    Q_OBJECT

private slots:
    void archIsoMatches();
    void ubuntuIsoMatches();
    void linuxMintUsesMajorVersionPath();
    void openSuseLeapMatches();
    void openSuseTumbleweedMatches();
    void unknownIsoReturnsNullopt();
    void knownPublisherIdsIncludesNewPublishers();
};

void TestIsoCatalog::archIsoMatches()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/mnt/usb/archlinux-2024.11.01-x86_64.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("archlinux"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("sha256sums.txt")));
}

void TestIsoCatalog::ubuntuIsoMatches()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/tmp/ubuntu-24.04.2-desktop-amd64.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("ubuntu"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("24.04.2")));
}

void TestIsoCatalog::linuxMintUsesMajorVersionPath()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/media/linuxmint-22.2-cinnamon-64bit.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("linuxmint"));
    QCOMPARE(match->checksumUrl,
             QStringLiteral("https://mirrors.kernel.org/linuxmint/stable/22/sha256sum.txt"));
    QCOMPARE(match->signatureUrl,
             QStringLiteral("https://mirrors.kernel.org/linuxmint/stable/22/sha256sum.txt.gpg"));
}

void TestIsoCatalog::openSuseLeapMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/iso/openSUSE-Leap-15.6-DVD-x86_64-Media.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("opensuse-leap"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("leap/15.6")));
}

void TestIsoCatalog::openSuseTumbleweedMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/iso/openSUSE-Tumbleweed-DVD-x86_64-Current.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("opensuse-tumbleweed"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("tumbleweed")));
}

void TestIsoCatalog::unknownIsoReturnsNullopt()
{
    QVERIFY(!IsoCatalog::matchIso(QStringLiteral("/tmp/random-live-1.0.iso")).has_value());
}

void TestIsoCatalog::knownPublisherIdsIncludesNewPublishers()
{
    const QStringList ids = IsoCatalog::knownPublisherIds();
    QVERIFY(ids.contains(QStringLiteral("linuxmint")));
    QVERIFY(ids.contains(QStringLiteral("opensuse-leap")));
    QVERIFY(ids.contains(QStringLiteral("opensuse-tumbleweed")));
}

QTEST_MAIN(TestIsoCatalog)
#include "test_iso_catalog.moc"
