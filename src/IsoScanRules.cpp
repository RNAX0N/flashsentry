#include "IsoScanRules.h"

#include "IsoCatalog.h"

#include <QDir>
#include <QFileInfo>

namespace FlashSentry {

namespace {

bool pathHasComponent(const QString& absolutePath, const QString& component)
{
    const QStringList parts =
        QDir::cleanPath(absolutePath).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part.compare(component, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

bool IsoScanRules::isReservedMultibootDirectory(const QString& dirName)
{
    static const QStringList reserved = {
        QStringLiteral("ventoy"),
        QStringLiteral(".ventoy"),
        QStringLiteral(".Ventoy"),
        QStringLiteral("EFI"),
        QStringLiteral("efi"),
        QStringLiteral("boot"),
        QStringLiteral("Boot"),
        QStringLiteral("grub"),
        QStringLiteral("grub2"),
        QStringLiteral("EFI-BOOT"),
        QStringLiteral("e2b"),
        QStringLiteral("_iso"),
        QStringLiteral("_ISO"),
        QStringLiteral("multiboot"),
        QStringLiteral("multibootusb"),
        QStringLiteral("System Volume Information"),
        QStringLiteral("$RECYCLE.BIN"),
        QStringLiteral("lost+found"),
        QStringLiteral(".Trash-000"),
        QStringLiteral(".Trashes"),
        QStringLiteral(".Spotlight-V100"),
        QStringLiteral(".fseventsd"),
        QStringLiteral(".TemporaryItems"),
    };
    for (const QString& name : reserved) {
        if (dirName.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (name.endsWith(QLatin1Char('-')) && dirName.startsWith(name, Qt::CaseInsensitive)) {
            return true;
        }
    }
    if (dirName.startsWith(QStringLiteral(".Trash-"), Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

bool IsoScanRules::isExcludedImagePath(const QString& absoluteFilePath)
{
    if (pathHasComponent(absoluteFilePath, QStringLiteral("ventoy"))
        || pathHasComponent(absoluteFilePath, QStringLiteral(".ventoy"))) {
        return true;
    }
    if (pathHasComponent(absoluteFilePath, QStringLiteral("EFI"))
        || pathHasComponent(absoluteFilePath, QStringLiteral("efi"))) {
        return true;
    }
    if (pathHasComponent(absoluteFilePath, QStringLiteral("e2b"))
        || pathHasComponent(absoluteFilePath, QStringLiteral("_iso"))
        || pathHasComponent(absoluteFilePath, QStringLiteral("_ISO"))) {
        return true;
    }
    return false;
}

MultibootLayout IsoScanRules::detectMultibootLayout(const QString& mountPoint)
{
    MultibootLayout layout;
    const QDir root(mountPoint);
    if (!root.exists()) {
        return layout;
    }

    const bool ventoyJson =
        root.exists(QStringLiteral("ventoy.json"))
        || root.exists(QStringLiteral("ventoy/ventoy.json"))
        || root.exists(QStringLiteral(".ventoy"));
    const bool ventoyDir = root.exists(QStringLiteral("ventoy"));
    const bool e2b = root.exists(QStringLiteral("_ISO")) || root.exists(QStringLiteral("e2b"));
    const bool grubIso = root.exists(QStringLiteral("boot/grub")) && root.exists(QStringLiteral("iso"));

    if (ventoyJson || (ventoyDir && root.exists(QStringLiteral("ventoy/ventoy.json")))) {
        layout.tool = MultibootTool::Ventoy;
        layout.dataPartition = true;
        layout.summary = QStringLiteral("Ventoy data partition");
        return layout;
    }

    if (e2b) {
        layout.tool = MultibootTool::Easy2Boot;
        layout.dataPartition = true;
        layout.summary = QStringLiteral("Easy2Boot-style layout");
        return layout;
    }

    if (grubIso) {
        layout.tool = MultibootTool::GrubMultiboot;
        layout.dataPartition = true;
        layout.summary = QStringLiteral("GRUB multiboot layout");
        return layout;
    }

    const bool hasEfi = root.exists(QStringLiteral("EFI")) || root.exists(QStringLiteral("efi"));
    const bool hasBoot = root.exists(QStringLiteral("boot")) || root.exists(QStringLiteral("Boot"));
    if ((hasEfi || hasBoot) && ventoyDir && !ventoyJson) {
        layout.tool = MultibootTool::Ventoy;
        layout.espOrBootOnly = true;
        layout.summary = QStringLiteral("Ventoy EFI/boot partition (no ISO scan)");
    }

    return layout;
}

bool IsoScanRules::shouldSkipAutoVerifyPartition(const QString& mountPoint, uint64_t sizeBytes,
                                                 int imageCount)
{
    if (imageCount > 0) {
        return false;
    }
    const MultibootLayout layout = detectMultibootLayout(mountPoint);
    if (layout.espOrBootOnly) {
        return true;
    }
    const QDir root(mountPoint);
    const bool hasEfiOnly =
        (root.exists(QStringLiteral("EFI")) || root.exists(QStringLiteral("efi")))
        && !root.exists(QStringLiteral("ventoy.json"));
    constexpr uint64_t kSmallPartitionBytes = 600ULL * 1024ULL * 1024ULL;
    if (hasEfiOnly && sizeBytes > 0 && sizeBytes < kSmallPartitionBytes) {
        return true;
    }
    return false;
}

QString IsoScanRules::coexistenceNote(MultibootTool tool)
{
    switch (tool) {
        case MultibootTool::Ventoy:
            return QStringLiteral(
                "Ventoy: FlashSentry only reads image files on the data partition. It does not "
                "modify ventoy/ config, boot menus, or the EFI partition. Mount options "
                "(noexec) do not affect Ventoy booting.");
        case MultibootTool::Easy2Boot:
            return QStringLiteral(
                "Easy2Boot: reserved _ISO/e2b folders are skipped; place distros outside those "
                "trees or use sidecars next to each image.");
        case MultibootTool::GrubMultiboot:
            return QStringLiteral(
                "GRUB multiboot: boot/ and grub/ trees are skipped during ISO discovery.");
        case MultibootTool::None:
            return QString();
    }
    return {};
}

} // namespace FlashSentry
