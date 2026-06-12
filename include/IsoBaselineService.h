#pragma once

#include "Types.h"

#include <QHash>
#include <QList>
#include <QString>

namespace FlashSpartan {

class IsoBaselineService {
public:
    struct ProcessingOutcome {
        QList<IsoVerifyResult> results;
        QList<IsoImageBaseline> updatedBaselines;
        bool baselinesChanged = false;
    };

    static QString relativeImagePath(const QString& mountPoint, const QString& isoPath);
    static QHash<QString, IsoImageBaseline> baselinesByRelativePath(
        const QList<IsoImageBaseline>& baselines);
    static std::optional<IsoImageBaseline> findBaseline(const QList<IsoImageBaseline>& baselines,
                                                        const QString& relativePath);

    static ProcessingOutcome process(const QString& mountPoint,
                                     const QList<IsoVerifyResult>& results,
                                     const QList<IsoImageBaseline>& existingBaselines,
                                     bool compareBaselines, bool storeBaselines);
};

} // namespace FlashSpartan
