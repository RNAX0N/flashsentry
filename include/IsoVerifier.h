#pragma once

#include "IsoVerifyOptions.h"
#include "Types.h"

#include <QString>
#include <QStringList>

namespace FlashSentry {

/**
 * @brief Fully automated ISO verification: local hash, remote publisher checksums, GPG.
 */
class IsoVerifier {
public:
    struct MountScanResult {
        QString mountPoint;
        QString deviceNode;
        QStringList isoPaths;
        QString layoutNote;
        bool looksLikeDdIsoStick = false;
    };

    static MountScanResult scanMountPoint(const QString& mountPoint);

    static QStringList findIsoFiles(const QString& directory);

    static IsoVerifyResult verifyIso(const QString& isoPath,
                                     const QString& mountPoint = {},
                                     const QString& deviceNode = {});

    static IsoVerifyResult verifyIsoAutomated(const QString& isoPath,
                                              const QString& mountPoint = {},
                                              const QString& deviceNode = {});

    static QList<IsoVerifyResult> verifyDirectory(const QString& directory);

    static QList<IsoVerifyResult> verifyMountPoint(const QString& mountPoint,
                                                   const QString& deviceNode = {});

    static IsoVerifyOptions& verifyOptions();
    static void setVerifyOptions(const IsoVerifyOptions& options);

    static bool mountScanHasFailures(const QList<IsoVerifyResult>& results);
};

} // namespace FlashSentry
