#pragma once

#include <QString>

namespace FlashSentry {

/** Left-rail application modules (content pages in the main stack). */
enum class AppPage {
    UsbMonitor = 0,
    DeviceHistory,
    AllowBlockList,
    Alerts,
    Reports,
    Settings,
    About,
};

inline QString appPageLabel(AppPage page)
{
    switch (page) {
        case AppPage::UsbMonitor: return QStringLiteral("USB Monitor");
        case AppPage::DeviceHistory: return QStringLiteral("Device History");
        case AppPage::AllowBlockList: return QStringLiteral("Allow/Block List");
        case AppPage::Alerts: return QStringLiteral("Alerts");
        case AppPage::Reports: return QStringLiteral("Reports");
        case AppPage::Settings: return QStringLiteral("Settings");
        case AppPage::About: return QStringLiteral("About");
    }
    return QStringLiteral("USB Monitor");
}

} // namespace FlashSentry
