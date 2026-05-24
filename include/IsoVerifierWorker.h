#pragma once

#include "Types.h"

#include <QObject>
#include <QString>

namespace FlashSentry {

/**
 * @brief Runs ISO verification on a background thread (mount-triggered or manual).
 */
class IsoVerifierWorker : public QObject {
    Q_OBJECT

public:
    explicit IsoVerifierWorker(QObject* parent = nullptr);

    void verifyMountPoint(const QString& mountPoint, const QString& deviceNode);
    void verifyDirectory(const QString& directory, const QString& deviceNode = {});

    void cancel();

signals:
    void verificationStarted(const QString& mountPoint, const QString& deviceNode);
    void verificationProgress(const QString& message);
    void verificationFinished(const QString& mountPoint, const QString& deviceNode,
                              const QList<IsoVerifyResult>& results);
    void verificationFailed(const QString& mountPoint, const QString& error);

private:
    bool m_cancelled = false;
};

} // namespace FlashSentry
