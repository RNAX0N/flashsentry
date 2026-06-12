#pragma once

#include <QString>

namespace FlashSpartan {

/** User-facing UDisks/D-Bus error text (testable without a live bus). */
class MountDBusUtil {
public:
    static QString formatMountError(const QString& dbusMessage);
};

} // namespace FlashSpartan
