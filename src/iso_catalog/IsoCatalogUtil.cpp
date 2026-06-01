#include "IsoCatalog.h"

namespace FlashSpartan {

bool IsoCatalog::isVerifiableImageFileName(const QString& fileName)
{
    static const QRegularExpression re(
        QStringLiteral("\\.(iso|img\\.xz|img|zip)$"), QRegularExpression::CaseInsensitiveOption);
    return re.match(fileName).hasMatch();
}

QStringList IsoCatalog::knownPublisherIds()
{
    return {QStringLiteral("archlinux"),       QStringLiteral("ubuntu"),
            QStringLiteral("kubuntu"),           QStringLiteral("xubuntu"),
            QStringLiteral("lubuntu"),           QStringLiteral("ubuntu-mate"),
            QStringLiteral("ubuntustudio"),      QStringLiteral("debian"),
            QStringLiteral("fedora"),            QStringLiteral("linuxmint"),
            QStringLiteral("opensuse-leap"),     QStringLiteral("opensuse-tumbleweed"),
            QStringLiteral("manjaro"),           QStringLiteral("kali"),
            QStringLiteral("centos-stream"),     QStringLiteral("rocky"),
            QStringLiteral("almalinux"),         QStringLiteral("elementary"),
            QStringLiteral("pop-os"),            QStringLiteral("endeavouros"),
            QStringLiteral("garuda"),            QStringLiteral("cachyos"),
            QStringLiteral("nobara"),            QStringLiteral("raspios"),
            QStringLiteral("ubuntu-rpi"),        QStringLiteral("alpine"),
            QStringLiteral("voidlinux"),         QStringLiteral("armbian"),
            QStringLiteral("nixos"),             QStringLiteral("microsoft-windows")};
}

} // namespace FlashSpartan
