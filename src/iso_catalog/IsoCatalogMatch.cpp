#include "IsoCatalog.h"
#include "IsoCatalogInternal.h"
#include "IsoCatalogManifest.h"

#include <QFileInfo>

namespace FlashSpartan {

std::optional<IsoPublisherMatch> IsoCatalog::matchIso(const QString& isoPath)
{
    const QFileInfo fi(isoPath);
    const QString name = fi.fileName();

    {
        static const QRegularExpression archRe(
            QStringLiteral("^archlinux-(.+)-(x86_64|aarch64)\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = archRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeArch(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression ubuntuMateRe(
            QStringLiteral("^ubuntu-mate-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = ubuntuMateRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCdimageUbuntuFlavor(QStringLiteral("ubuntu-mate"), QStringLiteral("Ubuntu MATE"),
                                           QStringLiteral("ubuntu-mate/releases"), name, m.captured(1));
        }
    }

    {
        static const QRegularExpression ubuntuStudioRe(
            QStringLiteral("^ubuntustudio-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = ubuntuStudioRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCdimageUbuntuFlavor(QStringLiteral("ubuntustudio"), QStringLiteral("Ubuntu Studio"),
                                           QStringLiteral("ubuntustudio/releases"), name, m.captured(1));
        }
    }

    {
        static const QRegularExpression kubuntuRe(
            QStringLiteral("^kubuntu-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = kubuntuRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCdimageUbuntuFlavor(QStringLiteral("kubuntu"), QStringLiteral("Kubuntu"),
                                           QStringLiteral("kubuntu/releases"), name, m.captured(1));
        }
    }

    {
        static const QRegularExpression xubuntuRe(
            QStringLiteral("^xubuntu-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = xubuntuRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCdimageUbuntuFlavor(QStringLiteral("xubuntu"), QStringLiteral("Xubuntu"),
                                           QStringLiteral("xubuntu/releases"), name, m.captured(1));
        }
    }

    {
        static const QRegularExpression lubuntuRe(
            QStringLiteral("^lubuntu-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = lubuntuRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCdimageUbuntuFlavor(QStringLiteral("lubuntu"), QStringLiteral("Lubuntu"),
                                           QStringLiteral("lubuntu/releases"), name, m.captured(1));
        }
    }

    {
        static const QRegularExpression ubuntuRe(
            QStringLiteral("^ubuntu-(\\d+\\.\\d+(?:\\.\\d+)?).+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = ubuntuRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeUbuntu(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression kaliRe(
            QStringLiteral("^kali-linux-(\\d+\\.\\d+).+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = kaliRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeKali(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression centosStreamRe(
            QStringLiteral("^CentOS-Stream-(\\d+)-x86_64.+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = centosStreamRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCentOsStream(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression elementaryRe(
            QStringLiteral("^elementaryos-(\\d+\\.\\d+)-amd64\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = elementaryRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeElementary(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression rockyRe(
            QStringLiteral("^Rocky-(\\d+)(?:\\.(\\d+))?-x86_64.+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = rockyRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeRocky(
                name, IsoCatalogInternal::rockyAlmaVersionPath(m.captured(1), m.captured(2)));
        }
    }

    {
        static const QRegularExpression almaRe(
            QStringLiteral("^AlmaLinux-(\\d+)(?:\\.(\\d+))?-x86_64.+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = almaRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeAlmaLinux(
                name, IsoCatalogInternal::rockyAlmaVersionPath(m.captured(1), m.captured(2)));
        }
    }

    {
        static const QRegularExpression popFullRe(
            QStringLiteral("^pop-os_(\\d+\\.\\d+)_amd64_([a-z]+)_(\\d+)\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = popFullRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makePopOs(name, m.captured(1), m.captured(2), m.captured(3));
        }
    }

    {
        static const QRegularExpression popSimpleRe(
            QStringLiteral("^pop-os_(\\d+\\.\\d+)_amd64\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = popSimpleRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makePopOsSimple(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression garudaRe(
            QStringLiteral("^garuda-([a-z0-9]+)-linux-zen-(\\d{6})\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = garudaRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeGaruda(name, m.captured(1).toLower(), m.captured(2));
        }
    }

    {
        static const QRegularExpression cachyosRe(
            QStringLiteral("^cachyos-([a-z0-9-]+)-linux-(\\d{6})\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = cachyosRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeCachyOs(name, m.captured(1).toLower(), m.captured(2));
        }
    }

    {
        static const QRegularExpression nobaraRe(
            QStringLiteral("^Nobara-\\d+-.+-\\d{4}-\\d{2}-\\d{2}\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        if (nobaraRe.match(name).hasMatch()) {
            return IsoCatalogInternal::makeNobara(name);
        }
    }

    {
        static const QRegularExpression endeavourRe(
            QStringLiteral("^endeavouros-(\\d+\\.\\d+\\.\\d+)-x86_64\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = endeavourRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeEndeavourOs(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression debianRe(
            QStringLiteral("^debian-(\\d+(?:\\.\\d+)+).+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = debianRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeDebian(name, m.captured(1));
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
            return IsoCatalogInternal::makeFedora(name, ver);
        }
    }

    {
        static const QRegularExpression manjaroRe(
            QStringLiteral("^manjaro-(\\w+)-(\\d+\\.\\d+\\.\\d+)-.+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = manjaroRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeManjaro(name, m.captured(1).toLower(), m.captured(2));
        }
    }

    {
        static const QRegularExpression mintRe(
            QStringLiteral("^linuxmint-(\\d+)(?:\\.\\d+)?-.+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = mintRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeLinuxMint(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression leapRe(
            QStringLiteral("^openSUSE-Leap-(\\d+\\.\\d+).+\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = leapRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeOpenSuseLeap(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression tumbleRe(
            QStringLiteral("^openSUSE-Tumbleweed-.+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        if (tumbleRe.match(name).hasMatch()) {
            return IsoCatalogInternal::makeOpenSuseTumbleweed(name);
        }
    }

    {
        static const QRegularExpression raspiosRe(
            QStringLiteral("^(\\d{4}-\\d{2}-\\d{2})-raspios-(.+)\\.(img\\.xz|zip)$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = raspiosRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeRaspiosOs(name, m.captured(1), m.captured(2));
        }
    }

    {
        static const QRegularExpression ubuntuRpiRe(
            QStringLiteral("^ubuntu-(\\d+\\.\\d+(?:\\.\\d+)?)-preinstalled-.+arm64\\+raspi.*\\.img\\.xz$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = ubuntuRpiRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeUbuntuRpi(name, m.captured(1));
        }
    }

    {
        static const QRegularExpression alpineRe(
            QStringLiteral("^alpine-(\\w+)-(\\d+\\.\\d+\\.\\d+)-(\\w+)\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = alpineRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeAlpine(name, m.captured(2), m.captured(3));
        }
    }

    {
        static const QRegularExpression voidRe(
            QStringLiteral("^void-live-.+\\.iso$"), QRegularExpression::CaseInsensitiveOption);
        if (voidRe.match(name).hasMatch()) {
            return IsoCatalogInternal::makeVoidLinux(name);
        }
    }

    {
        static const QRegularExpression armbianRe(
            QStringLiteral("^Armbian_.+\\.img\\.xz$"), QRegularExpression::CaseInsensitiveOption);
        if (armbianRe.match(name).hasMatch()) {
            return IsoCatalogInternal::makeArmbian(name);
        }
    }

    {
        static const QRegularExpression nixosRe(
            QStringLiteral("^nixos-(\\d+\\.\\d+(?:\\.\\d+)?)-([a-z0-9_-]+)-x86_64-linux\\.iso$"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = nixosRe.match(name);
        if (m.hasMatch()) {
            return IsoCatalogInternal::makeNixos(name, QStringLiteral("nixos-%1").arg(m.captured(1)), m.captured(2));
        }
    }

    if (auto manifest = IsoCatalogManifest::lookup(name)) {
        return manifest;
    }

    return std::nullopt;
}


} // namespace FlashSpartan
