#include <QtTest>
#include "IsoCatalog.h"
#include "IsoCatalogManifest.h"

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
    void manjaroPerFileArtifacts();
    void kaliMatches();
    void rockyMatches();
    void popOsMatches();
    void kubuntuMatches();
    void centosStreamMatches();
    void elementaryPerFileArtifacts();
    void knownPublisherIdsCount();
    void garudaMatches();
    void cachyosPerFileArtifacts();
    void nobaraMatches();
    void raspiosMatches();
    void ubuntuRpiMatches();
    void windowsManifestEmbedded();
    void windowsManifestHintOnly();
    void armbianMatches();
    void verifiableImageExtensions();
    void embeddedManifestIntegrity();
    void publisherFilenameTable_data();
    void publisherFilenameTable();
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
    QVERIFY(ids.contains(QStringLiteral("manjaro")));
    QVERIFY(ids.contains(QStringLiteral("kali")));
    QVERIFY(ids.contains(QStringLiteral("rocky")));
    QVERIFY(ids.contains(QStringLiteral("centos-stream")));
    QVERIFY(ids.contains(QStringLiteral("elementary")));
    QVERIFY(ids.contains(QStringLiteral("garuda")));
    QVERIFY(ids.contains(QStringLiteral("cachyos")));
    QVERIFY(ids.contains(QStringLiteral("nobara")));
}

void TestIsoCatalog::manjaroPerFileArtifacts()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/usb/manjaro-kde-25.0.0-250527-linux612.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("manjaro"));
    QVERIFY(match->perFileArtifacts);
    QVERIFY(match->checksumUrl.endsWith(QStringLiteral(".iso.sha256")));
    QVERIFY(match->signatureUrl.endsWith(QStringLiteral(".iso.sig")));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("/kde/")));
}

void TestIsoCatalog::kaliMatches()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/usb/kali-linux-2024.4-live-amd64.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("kali"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("kali-2024.4")));
}

void TestIsoCatalog::rockyMatches()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/iso/Rocky-9.4-x86_64-minimal.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("rocky"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("rocky/9.4")));
    QVERIFY(match->checksumUrl.endsWith(QStringLiteral("CHECKSUM")));
}

void TestIsoCatalog::popOsMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/usb/pop-os_22.04_amd64_intel_35.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("pop-os"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("/intel/35/SHA256SUMS")));
}

void TestIsoCatalog::kubuntuMatches()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/tmp/kubuntu-24.04-desktop-amd64.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("kubuntu"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("cdimage.ubuntu.com/kubuntu")));
}

void TestIsoCatalog::centosStreamMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/iso/CentOS-Stream-9-x86_64-dvd1.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("centos-stream"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("9-stream/isos")));
}

void TestIsoCatalog::elementaryPerFileArtifacts()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/usb/elementaryos-7.1-amd64.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("elementary"));
    QVERIFY(match->perFileArtifacts);
    QVERIFY(match->checksumUrl.endsWith(QStringLiteral("elementaryos-7.1-amd64.iso.sha256")));
}

void TestIsoCatalog::knownPublisherIdsCount()
{
    QCOMPARE(IsoCatalog::knownPublisherIds().size(), 30);
}

void TestIsoCatalog::garudaMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/iso/garuda-hyprland-linux-zen-250308.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("garuda"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("garuda/hyprland/250308")));
    QVERIFY(match->checksumUrl.endsWith(QStringLiteral(".iso.sha256")));
}

void TestIsoCatalog::cachyosPerFileArtifacts()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/usb/cachyos-desktop-linux-260308.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("cachyos"));
    QVERIFY(match->perFileArtifacts);
    QVERIFY(match->checksumUrl.contains(QStringLiteral("build.cachyos.org/ISO/desktop/260308")));
    QVERIFY(match->signatureUrl.endsWith(QStringLiteral(".iso.sig")));
}

void TestIsoCatalog::nobaraMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/usb/Nobara-43-Official-2026-04-19.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("nobara"));
    QVERIFY(match->checksumUrl.endsWith(QStringLiteral(".iso.sha256sum")));
    QCOMPARE(match->releaseLabel, QStringLiteral("43 Official"));
}

void TestIsoCatalog::raspiosMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/pi/2024-11-19-raspios-bookworm-arm64.img.xz"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("raspios"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("raspios_arm64-2024-11-19")));
    QVERIFY(match->checksumUrl.endsWith(QStringLiteral(".img.xz.sha256")));
}

void TestIsoCatalog::ubuntuRpiMatches()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral(
        "/pi/ubuntu-24.04.3-preinstalled-server-arm64+raspi.img.xz"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("ubuntu-rpi"));
    QVERIFY(match->checksumUrl.contains(QStringLiteral("releases/24.04.3/release/SHA256SUMS")));
}

void TestIsoCatalog::windowsManifestEmbedded()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/usb/Win11_24H2_English_x64.iso"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("microsoft-windows"));
    QVERIFY(!match->embeddedSha256.isEmpty());
    QCOMPARE(match->embeddedSha256,
             QStringLiteral("41196290521b7e4f814aca30c2cc4c7fab1e3076439418673b90954a1ffc54"));
}

void TestIsoCatalog::windowsManifestHintOnly()
{
    const auto match = IsoCatalog::matchIso(QStringLiteral("/usb/Win11_25H2_English_x64.iso"));
    QVERIFY(match.has_value());
    QVERIFY(match->hintOnly);
    QVERIFY(match->embeddedSha256.isEmpty());
}

void TestIsoCatalog::armbianMatches()
{
    const auto match = IsoCatalog::matchIso(
        QStringLiteral("/usb/Armbian_25.2.1_Odroidn2_bookworm_current_6.12.13.img.xz"));
    QVERIFY(match.has_value());
    QCOMPARE(match->publisherId, QStringLiteral("armbian"));
    QVERIFY(match->checksumUrl.isEmpty());
}

void TestIsoCatalog::verifiableImageExtensions()
{
    QVERIFY(IsoCatalog::isVerifiableImageFileName(QStringLiteral("debian.iso")));
    QVERIFY(IsoCatalog::isVerifiableImageFileName(QStringLiteral("pi.img.xz")));
    QVERIFY(IsoCatalog::isVerifiableImageFileName(QStringLiteral("disk.img")));
    QVERIFY(IsoCatalog::isVerifiableImageFileName(QStringLiteral("legacy.zip")));
    QVERIFY(!IsoCatalog::isVerifiableImageFileName(QStringLiteral("notes.txt")));
    QVERIFY(!IsoCatalog::isVerifiableImageFileName(QStringLiteral("image.iso.txt")));
}

void TestIsoCatalog::embeddedManifestIntegrity()
{
    IsoCatalogManifest::ensureLoaded();
    QVERIFY(IsoCatalogManifest::lastEmbeddedIntegrityOk());
    QVERIFY(IsoCatalogManifest::entryCount() >= 4);
    if (QFile::exists(QStringLiteral(":/iso-catalog/iso-catalog/embedded-manifest.json.asc"))) {
        QVERIFY2(IsoCatalogManifest::lastEmbeddedIntegrityOk(),
                 "Embedded manifest SHA-256 or OpenPGP integrity check failed");
    }
}

void TestIsoCatalog::publisherFilenameTable_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("publisherId");

    QTest::newRow("arch") << QStringLiteral("/mnt/archlinux-2024.11.01-x86_64.iso")
                          << QStringLiteral("archlinux");
    QTest::newRow("ubuntu") << QStringLiteral("/tmp/ubuntu-24.04.2-desktop-amd64.iso")
                            << QStringLiteral("ubuntu");
    QTest::newRow("debian") << QStringLiteral("/iso/debian-12.5.0-amd64-netinst.iso")
                            << QStringLiteral("debian");
    QTest::newRow("fedora") << QStringLiteral("/iso/Fedora-Workstation-Live-41-1.4.x86_64.iso")
                            << QStringLiteral("fedora");
    QTest::newRow("manjaro") << QStringLiteral("/usb/manjaro-kde-25.0.0-250527-linux612.iso")
                             << QStringLiteral("manjaro");
    QTest::newRow("kali") << QStringLiteral("/iso/kali-linux-2024.4-live-amd64.iso")
                          << QStringLiteral("kali");
    QTest::newRow("rocky") << QStringLiteral("/iso/Rocky-9.4-x86_64-minimal.iso")
                           << QStringLiteral("rocky");
    QTest::newRow("garuda") << QStringLiteral("/iso/garuda-hyprland-linux-zen-250308.iso")
                            << QStringLiteral("garuda");
    QTest::newRow("nobara") << QStringLiteral("/iso/Nobara-43-Official-2026-04-19.iso")
                            << QStringLiteral("nobara");
    QTest::newRow("raspios") << QStringLiteral("/pi/2024-11-19-raspios-bookworm-arm64.img.xz")
                             << QStringLiteral("raspios");
    QTest::newRow("alpine") << QStringLiteral("/iso/alpine-standard-3.20.3-x86_64.iso")
                            << QStringLiteral("alpine");
    QTest::newRow("nixos") << QStringLiteral("/iso/nixos-24.05-minimal-x86_64-linux.iso")
                           << QStringLiteral("nixos");
}

void TestIsoCatalog::publisherFilenameTable()
{
    QFETCH(QString, path);
    QFETCH(QString, publisherId);
    const auto match = IsoCatalog::matchIso(path);
    QVERIFY2(match.has_value(), qPrintable(path));
    QCOMPARE(match->publisherId, publisherId);
}

QTEST_MAIN(TestIsoCatalog)
#include "test_iso_catalog.moc"
