#pragma once

#include "Types.h"

#include <QList>
#include <QString>

namespace FlashSpartan {

class IsoVerifyReport {
public:
    struct SummaryCounts {
        int passed = 0;
        int total = 0;
        int needsSidecar = 0;
    };

    static SummaryCounts countSummary(const QList<IsoVerifyResult>& results);
    static QString buildPlainText(const QList<IsoVerifyResult>& results);
    static QString buildCsv(const QList<IsoVerifyResult>& results);
    static QString buildHtml(const QList<IsoVerifyResult>& results);
    /** Compact JSON for scripting (`--json`). */
    static QString buildJson(const QList<IsoVerifyResult>& results);
    static QString summaryLine(const QList<IsoVerifyResult>& results);
};

} // namespace FlashSpartan
