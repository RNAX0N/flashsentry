#include "IsoCatalog.h"

#include <QFileInfo>
#include <optional>

namespace FlashSentry {

namespace {

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

} // namespace

std::optional<IsoPublisherMatch> IsoCatalog::matchIso(const QString& isoPath)
{
    const QFileInfo fi(isoPath);
    const QString name = fi.fileName();

    {
        static const QRegularExpression archRe(
            QStringLiteral("^archlinux-(.+)-(x86_64|aarch64)\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = archRe.match(name);
        if (m.hasMatch()) {
            return makeArch(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression ubuntuRe(
            QStringLiteral("^ubuntu-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = ubuntuRe.match(name);
        if (m.hasMatch()) {
            return makeUbuntu(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression debianRe(
            QStringLiteral("^debian-(\\d+(?:\\.\\d+)+).+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = debianRe.match(name);
        if (m.hasMatch()) {
            return makeDebian(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression fedoraRe(
            QStringLiteral("^Fedora-(.+)-\\d+.*\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = fedoraRe.match(name);
        if (m.hasMatch()) {
            QString ver = m.captured(1);
            if (ver.contains(QLatin1Char('.'))) {
                ver = ver.section(QLatin1Char('.'), 0, 1);
            }
            return makeFedora(name, ver);
        }
    }

    {
        static const QRegularExpression mintRe(
            QStringLiteral("^linuxmint-(\\d+)(?:\\.\\d+)?-.+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = mintRe.match(name);
        if (m.hasMatch()) {
            return makeLinuxMint(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression leapRe(
            QStringLiteral("^openSUSE-Leap-(\\d+\\.\\d+).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = leapRe.match(name);
        if (m.hasMatch()) {
            return makeOpenSuseLeap(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression tumbleRe(
            QStringLiteral("^openSUSE-Tumbleweed-.+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        if (tumbleRe.match(name).hasMatch()) {
            return makeOpenSuseTumbleweed(name);
        }
    }

    return std::nullopt;
}

QStringList IsoCatalog::knownPublisherIds()
{
    return {QStringLiteral("archlinux"), QStringLiteral("ubuntu"),
            QStringLiteral("debian"), QStringLiteral("fedora"),
            QStringLiteral("linuxmint"), QStringLiteral("opensuse-leap"),
            QStringLiteral("opensuse-tumbleweed")};
}

} // namespace FlashSentry
