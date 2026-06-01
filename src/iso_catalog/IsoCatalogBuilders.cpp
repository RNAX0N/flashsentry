#include "IsoCatalogInternal.h"

#include <QRegularExpression>

namespace FlashSpartan {
namespace IsoCatalogInternal {

QString normalizeFingerprint(const QString& fp)
{
    QString s = fp;
    s.remove(QLatin1Char(' '));
    return s.toUpper();
}

IsoPublisherMatch makeArch(const QString& fileName, const QString& versionTag)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("archlinux");
    m.publisherName = QStringLiteral("Arch Linux");
    m.releaseLabel = versionTag;
    m.isoFileName = fileName;
    m.checksumUrl = QStringLiteral("https://geo.mirror.pkgbuild.com/iso/latest/sha256sums.txt");
    m.signatureUrl = QStringLiteral("https://geo.mirror.pkgbuild.com/iso/latest/sha256sums.txt.sig");
    m.signingKeyIds = {QStringLiteral("0x7949D089"), QStringLiteral("0x7A1B4E2D")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("4AA4 767B BC26 9466 99BE 394B 31DB D89E 5A2A 8E65")),
        normalizeFingerprint(QStringLiteral("6841 48ED 3E97 4B8F 27B2 9DF7 00F4 9D16 0A86 2172")),
    };
    return m;
}

IsoPublisherMatch makeUbuntu(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("ubuntu");
    m.publisherName = QStringLiteral("Ubuntu");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://releases.ubuntu.com/%1/").arg(version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.gpg");
    m.signingKeyIds = {QStringLiteral("0x843938DF")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("8439 38DF 228B 22A7 0FAC 1C9B 0C3A 2CEF 088A 431B")),
    };
    return m;
}

IsoPublisherMatch makeDebian(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("debian");
    m.publisherName = QStringLiteral("Debian");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://cdimage.debian.org/debian-cd/%1/amd64/iso-cd/")
                             .arg(version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.sign");
    m.signingKeyIds = {QStringLiteral("0xDF9B9C49")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("DF9B 9C49 1104 5C76 860F 4A44 0B50 6FB0 3B82 4E6B")),
    };
    return m;
}

IsoPublisherMatch makeLinuxMint(const QString& fileName, const QString& majorVersion)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("linuxmint");
    m.publisherName = QStringLiteral("Linux Mint");
    m.releaseLabel = majorVersion;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://mirrors.kernel.org/linuxmint/stable/%1/")
                             .arg(majorVersion);
    m.checksumUrl = base + QStringLiteral("sha256sum.txt");
    m.signatureUrl = base + QStringLiteral("sha256sum.txt.gpg");
    m.signingKeyIds = {QStringLiteral("0xA25BAE09")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("27DE B156 44C6 B3CF 3BD7 D291 300F 846B A25B AE09")),
    };
    return m;
}

IsoPublisherMatch makeOpenSuseLeap(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("opensuse-leap");
    m.publisherName = QStringLiteral("openSUSE Leap");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://download.opensuse.org/distribution/leap/%1/iso/")
                             .arg(version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.sign");
    m.signingKeyIds = {QStringLiteral("0x29d97ba47c215819")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("29D9 7BA4 7C21 8193 7701 6271 7370 352E 1C51 80BC")),
    };
    return m;
}

IsoPublisherMatch makeOpenSuseTumbleweed(const QString& fileName)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("opensuse-tumbleweed");
    m.publisherName = QStringLiteral("openSUSE Tumbleweed");
    m.releaseLabel = QStringLiteral("Current");
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://download.opensuse.org/tumbleweed/iso/");
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.sign");
    m.signingKeyIds = {QStringLiteral("0x29d97ba47c215819")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("29D9 7BA4 7C21 8193 7701 6271 7370 352E 1C51 80BC")),
    };
    return m;
}

IsoPublisherMatch makeManjaro(const QString& fileName, const QString& edition,
                              const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("manjaro");
    m.publisherName = QStringLiteral("Manjaro");
    m.releaseLabel = QStringLiteral("%1 %2").arg(edition, version);
    m.isoFileName = fileName;
    m.perFileArtifacts = true;
    const QString base = QStringLiteral("https://download.manjaro.org/manjaro/iso/%1/%2/")
                             .arg(version, edition);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = base + fileName + QStringLiteral(".sig");
    m.signingKeyIds = {QStringLiteral("0x5EBE35B2")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("518C BA83 A18D 0F97 4D17 FBB6 5F1E 8D0A 0C5C 2E5B")),
    };
    return m;
}

IsoPublisherMatch makeFedora(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("fedora");
    m.publisherName = QStringLiteral("Fedora");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    m.checksumUrl = QStringLiteral("https://download.fedoraproject.org/pub/fedora/linux/releases/%1/Server/x86_64/iso/Fedora-%2-1.6-x86_64-CHECKSUM")
                        .arg(version, version);
    m.signatureUrl = m.checksumUrl;
    m.signingKeyIds = {QStringLiteral("0x115DF9B1")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("115D F9BE 8CBA B43E 943B 0276 F8C2 918A 4320 5F2E")),
    };
    return m;
}

IsoPublisherMatch makeCdimageUbuntuFlavor(const QString& publisherId, const QString& publisherName,
                                          const QString& cdimagePath, const QString& fileName,
                                          const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = publisherId;
    m.publisherName = publisherName;
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://cdimage.ubuntu.com/%1/%2/release/")
                             .arg(cdimagePath, version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.gpg");
    m.signingKeyIds = {QStringLiteral("0x843938DF")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("8439 38DF 228B 22A7 0FAC 1C9B 0C3A 2CEF 088A 431B")),
    };
    return m;
}

IsoPublisherMatch makeKali(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("kali");
    m.publisherName = QStringLiteral("Kali Linux");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://cdimage.kali.org/kali-%1/").arg(version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.gpg");
    m.signingKeyIds = {QStringLiteral("0xED65462EC8D5E4C5")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("827C 8569 F251 8CC6 77FE CA1A ED65 462E C8D5 E4C5")),
    };
    return m;
}

IsoPublisherMatch makeRocky(const QString& fileName, const QString& versionPath)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("rocky");
    m.publisherName = QStringLiteral("Rocky Linux");
    m.releaseLabel = versionPath;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://download.rockylinux.org/pub/rocky/%1/isos/x86_64/")
                             .arg(versionPath);
    m.checksumUrl = base + QStringLiteral("CHECKSUM");
    m.signatureUrl = base + QStringLiteral("CHECKSUM");
    m.signingKeyIds = {QStringLiteral("0xB86B3716")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("BF18 AC28 7617 8908 D6E7 1267 D36C B86C B86B 3716")),
    };
    return m;
}

IsoPublisherMatch makeAlmaLinux(const QString& fileName, const QString& versionPath)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("almalinux");
    m.publisherName = QStringLiteral("AlmaLinux");
    m.releaseLabel = versionPath;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://repo.almalinux.org/almalinux/%1/isos/x86_64/")
                             .arg(versionPath);
    m.checksumUrl = base + QStringLiteral("CHECKSUM");
    m.signatureUrl = base + QStringLiteral("CHECKSUM");
    m.signingKeyIds = {QStringLiteral("0xB86B3716")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("BF18 AC28 7617 8908 D6E7 1267 D36C B86C B86B 3716")),
    };
    return m;
}

IsoPublisherMatch makePopOs(const QString& fileName, const QString& version,
                            const QString& variant, const QString& build)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("pop-os");
    m.publisherName = QStringLiteral("Pop!_OS");
    m.releaseLabel = QStringLiteral("%1 %2 build %3").arg(version, variant, build);
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://iso.pop-os.org/%1/amd64/%2/%3/")
                             .arg(version, variant, build);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.gpg");
    m.signingKeyIds = {QStringLiteral("0x204DD8AEC33A7AFF")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("63C4 6DF0 140D 7389 6142 9F4E 204D D8AE C33A 7AFF")),
    };
    return m;
}

IsoPublisherMatch makePopOsSimple(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("pop-os");
    m.publisherName = QStringLiteral("Pop!_OS");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://iso.pop-os.org/%1/amd64/").arg(version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.gpg");
    m.signingKeyIds = {QStringLiteral("0x204DD8AEC33A7AFF")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("63C4 6DF0 140D 7389 6142 9F4E 204D D8AE C33A 7AFF")),
    };
    return m;
}

IsoPublisherMatch makeCentOsStream(const QString& fileName, const QString& major)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("centos-stream");
    m.publisherName = QStringLiteral("CentOS Stream");
    m.releaseLabel = major;
    m.isoFileName = fileName;
    const QString versionPath = major + QStringLiteral("-stream");
    const QString base = QStringLiteral("https://mirror.stream.centos.org/%1/isos/x86_64/")
                             .arg(versionPath);
    m.checksumUrl = base + QStringLiteral("CHECKSUM");
    m.signatureUrl = base + QStringLiteral("CHECKSUM");
    m.signingKeyIds = {QStringLiteral("0x8483C65D")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("BF18 AC28 7617 8908 D6E7 1267 D36C B86C B86B 3716")),
    };
    return m;
}

IsoPublisherMatch makeElementary(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("elementary");
    m.publisherName = QStringLiteral("elementary OS");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    m.perFileArtifacts = true;
    const QString base = QStringLiteral("https://updates.elementary.io/stable/");
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = base + fileName + QStringLiteral(".asc");
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

IsoPublisherMatch makeGaruda(const QString& fileName, const QString& edition, const QString& date)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("garuda");
    m.publisherName = QStringLiteral("Garuda Linux");
    m.releaseLabel = QStringLiteral("%1 %2").arg(edition, date);
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://iso.builds.garudalinux.org/iso/garuda/%1/%2/")
                             .arg(edition, date);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = QString();
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

IsoPublisherMatch makeCachyOs(const QString& fileName, const QString& variant, const QString& date)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("cachyos");
    m.publisherName = QStringLiteral("CachyOS");
    m.releaseLabel = QStringLiteral("%1 %2").arg(variant, date);
    m.isoFileName = fileName;
    m.perFileArtifacts = true;
    const QString base = QStringLiteral("https://build.cachyos.org/ISO/%1/%2/").arg(variant, date);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = base + fileName + QStringLiteral(".sig");
    m.signingKeyIds = {QStringLiteral("0xF3B607488DB35A47")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("882D CFE4 8E20 51D4 8E25 62AB F3B6 0748 8DB3 5A47")),
    };
    return m;
}

IsoPublisherMatch makeNobara(const QString& fileName)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("nobara");
    m.publisherName = QStringLiteral("Nobara Linux");
    m.isoFileName = fileName;
    static const QRegularExpression nobaraRe(
        QStringLiteral("^Nobara-(\\d+)-(.+)-(\\d{4}-\\d{2}-\\d{2})\\.iso$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = nobaraRe.match(fileName);
    if (match.hasMatch()) {
        m.releaseLabel = QStringLiteral("%1 %2").arg(match.captured(1), match.captured(2));
    } else {
        m.releaseLabel = fileName;
    }
    const QString base = QStringLiteral("https://nobara-images.nobaraproject.org/");
    m.checksumUrl = base + fileName + QStringLiteral(".sha256sum");
    m.signatureUrl = QString();
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

QString raspiosMirrorTree(const QString& suffix)
{
    if (suffix.endsWith(QStringLiteral("-lite"))) {
        if (suffix.contains(QStringLiteral("arm64"))) {
            return QStringLiteral("raspios_lite_arm64");
        }
        return QStringLiteral("raspios_lite_armhf");
    }
    if (suffix.contains(QStringLiteral("full"))) {
        if (suffix.contains(QStringLiteral("arm64"))) {
            return QStringLiteral("raspios_full_arm64");
        }
        if (suffix.contains(QStringLiteral("armhf"))) {
            return QStringLiteral("raspios_full_armhf");
        }
    }
    if (suffix.contains(QStringLiteral("arm64"))) {
        return QStringLiteral("raspios_arm64");
    }
    if (suffix.contains(QStringLiteral("armhf"))) {
        return QStringLiteral("raspios_armhf");
    }
    return QStringLiteral("raspios_arm64");
}

IsoPublisherMatch makeRaspiosOs(const QString& fileName, const QString& date, const QString& suffix)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("raspios");
    m.publisherName = QStringLiteral("Raspberry Pi OS");
    m.releaseLabel = QStringLiteral("%1 %2").arg(date, suffix);
    m.isoFileName = fileName;
    const QString tree = raspiosMirrorTree(suffix);
    const bool legacy = suffix.contains(QStringLiteral("buster")) || date < QStringLiteral("2022-06-01");
    const QString host = legacy ? QStringLiteral("https://downloads.raspberrypi.org")
                                : QStringLiteral("https://downloads.raspberrypi.com");
    const QString base = host + QStringLiteral("/%1/images/%2-%3/").arg(tree, tree, date);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = base + fileName + QStringLiteral(".sig");
    m.signingKeyIds = {QStringLiteral("0xCF8A072D")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("FC8A 072D 3B48 8CE6 99B0 56B8 94A9 8445 94AE 2274")),
    };
    return m;
}

IsoPublisherMatch makeUbuntuRpi(const QString& fileName, const QString& version)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("ubuntu-rpi");
    m.publisherName = QStringLiteral("Ubuntu for Raspberry Pi");
    m.releaseLabel = version;
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://cdimage.ubuntu.com/releases/%1/release/").arg(version);
    m.checksumUrl = base + QStringLiteral("SHA256SUMS");
    m.signatureUrl = base + QStringLiteral("SHA256SUMS.gpg");
    m.signingKeyIds = {QStringLiteral("0x843938DF")};
    m.trustedFingerprints = {
        normalizeFingerprint(QStringLiteral("8439 38DF 228B 22A7 0FAC 1C9B 0C3A 2CEF 088A 431B")),
    };
    return m;
}

IsoPublisherMatch makeAlpine(const QString& fileName, const QString& version, const QString& arch)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("alpine");
    m.publisherName = QStringLiteral("Alpine Linux");
    m.releaseLabel = QStringLiteral("%1 %2").arg(version, arch);
    m.isoFileName = fileName;
    const QString majorMinor = version.section(QLatin1Char('.'), 0, 1);
    const QString base = QStringLiteral("https://dl-cdn.alpinelinux.org/alpine/v%1/releases/%2/")
                             .arg(majorMinor, arch);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = QString();
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

IsoPublisherMatch makeVoidLinux(const QString& fileName)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("voidlinux");
    m.publisherName = QStringLiteral("Void Linux");
    m.releaseLabel = fileName;
    m.isoFileName = fileName;
    m.checksumUrl = QStringLiteral("https://repo-default.voidlinux.org/live/current/sha256sum.txt");
    m.signatureUrl = QStringLiteral("https://repo-default.voidlinux.org/live/current/sha256sum.sig");
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

IsoPublisherMatch makeArmbian(const QString& fileName)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("armbian");
    m.publisherName = QStringLiteral("Armbian");
    m.releaseLabel = fileName;
    m.isoFileName = fileName;
    m.checksumUrl = QString();
    m.signatureUrl = QString();
    m.referenceUrl = QStringLiteral("https://www.armbian.com/download/");
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

IsoPublisherMatch makeNixos(const QString& fileName, const QString& channel, const QString& variant)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("nixos");
    m.publisherName = QStringLiteral("NixOS");
    m.releaseLabel = QStringLiteral("%1 %2").arg(channel, variant);
    m.isoFileName = fileName;
    const QString base = QStringLiteral("https://channels.nixos.org/%1/latest-nixos-%2/").arg(channel, variant);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = QString();
    m.signingKeyIds = {};
    m.trustedFingerprints = {};
    return m;
}

IsoPublisherMatch makeEndeavourOs(const QString& fileName, const QString& dateVersion)
{
    IsoPublisherMatch m;
    m.publisherId = QStringLiteral("endeavouros");
    m.publisherName = QStringLiteral("EndeavourOS");
    m.releaseLabel = dateVersion;
    m.isoFileName = fileName;
    m.perFileArtifacts = true;
    const QStringList parts = dateVersion.split(QLatin1Char('.'));
    const QString tag = parts.size() >= 2
        ? parts[0].mid(2) + QLatin1Char('-') + parts[1]
        : dateVersion;
    const QString base = QStringLiteral("https://github.com/endeavouros-team/ISO/releases/download/%1/")
                             .arg(tag);
    m.checksumUrl = base + fileName + QStringLiteral(".sha256");
    m.signatureUrl = base + fileName + QStringLiteral(".asc");
    m.signingKeyIds = {QStringLiteral("0x497F5E69B7440F2A")};
    m.trustedFingerprints = {};
    return m;
}

QString rockyAlmaVersionPath(const QString& major, const QString& minor)
{
    if (minor.isEmpty()) {
        return major;
    }
    return major + QLatin1Char('.') + minor;
}


} // namespace IsoCatalogInternal
} // namespace FlashSpartan
