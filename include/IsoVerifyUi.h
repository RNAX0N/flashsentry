#pragma once

#include "Types.h"

#include <QList>
#include <QString>

namespace FlashSpartan {

/**
 * User-facing labels, summaries, and guidance for ISO/image verification.
 * Technical terms (PASS, inconclusive, sidecar) are kept out of the main UI.
 */
class IsoVerifyUi {
public:
    enum class Outcome { Verified, Failed, NotVerified };

    static Outcome outcome(const IsoVerifyResult& result);
    static QString outcomeLabel(const IsoVerifyResult& result);
    static QString outcomeExplanation(const IsoVerifyResult& result);
    static QString nextStepHint(const IsoVerifyResult& result);

    static QString hashColumnText(const IsoVerifyResult& result);
    static QString pgpColumnText(const IsoVerifyResult& result);
    static QString keyColumnText(const IsoVerifyResult& result);

    static QString summaryLine(int passed, int total, int notVerified);
    static QString summaryLine(const QList<IsoVerifyResult>& results);

    static QString verifiedChipText(int count);
    static QString failedChipText(int count);
    static QString notVerifiedChipText(int count);

    static QString introHtml();
    static QString legendHtml();
    static QString tableHeaderTooltip(int column);

    static QString trayTitle();
    static QString trayMessage(const QString& deviceName, int passed, int total, int notVerified);
};

} // namespace FlashSpartan
