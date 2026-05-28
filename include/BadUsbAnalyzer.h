#pragma once

#include "Types.h"

#include <optional>

namespace FlashSentry::BadUsbAnalyzer {

BadUsbAnomalyResult analyzeConnect(const HidDeviceInfo& device,
                                   const std::optional<BadUsbBaselineEntry>& baseline,
                                   const QStringList& relatedStorageNodes,
                                   int recentConnectCount,
                                   const AppSettings& settings);

QString severityLabel(BadUsbSeverity severity);

} // namespace FlashSentry::BadUsbAnalyzer
