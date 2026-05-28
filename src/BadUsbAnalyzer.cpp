#include "BadUsbAnalyzer.h"

namespace FlashSentry::BadUsbAnalyzer {

namespace {

BadUsbAnomalyResult makeResult(const HidDeviceInfo& device, BadUsbSeverity severity,
                               const QString& ruleId, const QString& summary,
                               const QString& detail, const QStringList& relatedStorageNodes)
{
    BadUsbAnomalyResult result;
    result.anomalous = true;
    result.severity = severity;
    result.ruleId = ruleId;
    result.summary = summary;
    result.detail = detail;
    result.device = device;
    result.relatedStorageNodes = relatedStorageNodes;
    result.detectedAtUtc = QDateTime::currentDateTimeUtc();
    return result;
}

} // namespace

BadUsbAnomalyResult analyzeConnect(const HidDeviceInfo& device,
                                   const std::optional<BadUsbBaselineEntry>& baseline,
                                   const QStringList& relatedStorageNodes,
                                   int recentConnectCount,
                                   const AppSettings& settings)
{
    BadUsbAnomalyResult ok;
    ok.device = device;
    ok.relatedStorageNodes = relatedStorageNodes;
    ok.detectedAtUtc = QDateTime::currentDateTimeUtc();

    if (settings.badUsbAlertCompositeStorage && device.isKeyboard()
        && !relatedStorageNodes.isEmpty()) {
        return makeResult(
            device, BadUsbSeverity::Critical, QStringLiteral("composite-storage-keyboard"),
            QStringLiteral("USB HID keyboard appeared with mass-storage on the same device"),
            QStringLiteral("A keyboard interface and storage volume share identifiers. This is a "
                           "common BadUSB pattern and should be trusted only if expected."),
            relatedStorageNodes);
    }

    if (settings.badUsbAlertNewKeyboard && device.isKeyboard()
        && (!baseline.has_value() || !baseline->trusted)) {
        return makeResult(
            device, BadUsbSeverity::Warning, QStringLiteral("new-keyboard"),
            QStringLiteral("New untrusted USB keyboard detected"),
            QStringLiteral("Keyboards can inject keystrokes immediately after connection. Add this "
                           "device to the baseline only if you physically recognize it."),
            relatedStorageNodes);
    }

    if (settings.badUsbAlertInterfaceDrift && baseline.has_value()) {
        const QStringList expected = baseline->device.interfaceSignatures();
        const QStringList actual = device.interfaceSignatures();
        if (!expected.isEmpty() && expected != actual) {
            return makeResult(
                device, BadUsbSeverity::Warning, QStringLiteral("interface-drift"),
                QStringLiteral("USB HID interface set changed from baseline"),
                QStringLiteral("The same HID identity now exposes a different interface, driver, "
                               "or protocol set than the trusted baseline."),
                relatedStorageNodes);
        }
    }

    if (settings.badUsbAlertRapidReconnect && recentConnectCount >= 3) {
        return makeResult(
            device, BadUsbSeverity::Warning, QStringLiteral("rapid-reconnect"),
            QStringLiteral("USB HID device reconnected repeatedly"),
            QStringLiteral("Repeated disconnect/connect cycles can indicate firmware reset, "
                           "interface switching, or probing behavior."),
            relatedStorageNodes);
    }

    return ok;
}

QString severityLabel(BadUsbSeverity severity)
{
    switch (severity) {
        case BadUsbSeverity::Info: return QStringLiteral("Info");
        case BadUsbSeverity::Warning: return QStringLiteral("Warning");
        case BadUsbSeverity::Critical: return QStringLiteral("Critical");
    }
    return QStringLiteral("Unknown");
}

} // namespace FlashSentry::BadUsbAnalyzer
