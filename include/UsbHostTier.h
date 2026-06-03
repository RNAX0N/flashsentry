#pragma once

#include <QString>

namespace FlashSpartan {

/** How a USB host node should appear in the USB Monitor UI. */
enum class UsbHostTier {
    /** Hubs, controllers, composite interfaces, onboard laptop USB — tracked, not listed as drives. */
    InternalHost = 0,
    /** Cameras, security keys, phones, docked peripherals — optional secondary list. */
    ExternalPeripheral,
};

inline QString usbHostTierLabel(UsbHostTier tier)
{
    switch (tier) {
        case UsbHostTier::InternalHost:
            return QStringLiteral("Built-in / host USB");
        case UsbHostTier::ExternalPeripheral:
            return QStringLiteral("USB peripheral");
    }
    return QStringLiteral("USB");
}

} // namespace FlashSpartan
