#pragma once

#include "Types.h"

#include <QList>
#include <QString>

namespace FlashSentry {

class IsoVerifyReport {
public:
    static QString buildPlainText(const QList<IsoVerifyResult>& results);
    static QString buildCsv(const QList<IsoVerifyResult>& results);
    static QString buildHtml(const QList<IsoVerifyResult>& results);

    static QString summaryLine(const QList<IsoVerifyResult>& results);
};

} // namespace FlashSentry
