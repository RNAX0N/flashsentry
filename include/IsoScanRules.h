#pragma once

#include "Types.h"

#include <QString>

namespace FlashSpartan {

enum class MultibootTool {
    None,
    Ventoy,
    Easy2Boot,
    GrubMultiboot,
};

struct MultibootLayout {
    MultibootTool tool = MultibootTool::None;
    bool dataPartition = false;
    bool espOrBootOnly = false;
    QString summary;
};

/**
 * @brief Path rules for ISO scanning on multiboot sticks (Ventoy, Easy2Boot, etc.).
 *
 * Skips vendor boot/config trees so verification does not touch Ventoy metadata
 * or fight with multiboot layouts. Read-only hashing of user images is safe.
 */
class IsoScanRules {
public:
    /** Directory name at any depth that should not be entered when scanning. */
    static bool isReservedMultibootDirectory(const QString& dirName);

    /** Absolute path to an image file that should be ignored. */
    static bool isExcludedImagePath(const QString& absoluteFilePath);

    static MultibootLayout detectMultibootLayout(const QString& mountPoint);

    /**
     * Skip auto-verify on tiny EFI/Ventoy boot partitions (no user images).
     * @a sizeBytes partition size from udev (0 if unknown).
     */
    static bool shouldSkipAutoVerifyPartition(const QString& mountPoint, uint64_t sizeBytes,
                                              int imageCount);

    static QString coexistenceNote(MultibootTool tool);
};

} // namespace FlashSpartan
