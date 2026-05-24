#include "IsoVerifierWorker.h"
#include "IsoVerifier.h"

#include <QtConcurrent>

namespace FlashSentry {

IsoVerifierWorker::IsoVerifierWorker(QObject* parent)
    : QObject(parent)
{
}

void IsoVerifierWorker::cancel()
{
    m_cancelled = true;
}

void IsoVerifierWorker::verifyMountPoint(const QString& mountPoint, const QString& deviceNode)
{
    m_cancelled = false;
    emit verificationStarted(mountPoint, deviceNode);

    m_activeJob = QtConcurrent::run([this, mountPoint, deviceNode]() {
        if (m_cancelled) return;
        emit verificationProgress(QStringLiteral("Scanning %1 for ISO images…").arg(mountPoint));

        const QList<IsoVerifyResult> results = IsoVerifier::verifyMountPoint(mountPoint, deviceNode);
        if (m_cancelled) return;

        QMetaObject::invokeMethod(this, [this, mountPoint, deviceNode, results]() {
            emit verificationFinished(mountPoint, deviceNode, results);
        }, Qt::QueuedConnection);
    });
}

void IsoVerifierWorker::verifyDirectory(const QString& directory, const QString& deviceNode)
{
    m_cancelled = false;
    emit verificationStarted(directory, deviceNode);
    emit verificationProgress(QStringLiteral("Verifying ISOs in %1…").arg(directory));

    m_activeJob = QtConcurrent::run([this, directory, deviceNode]() {
        QList<IsoVerifyResult> results = IsoVerifier::verifyDirectory(directory);
        for (IsoVerifyResult& r : results) {
            r.deviceNode = deviceNode;
        }
        if (m_cancelled) return;
        QMetaObject::invokeMethod(this, [this, directory, deviceNode, results]() {
            emit verificationFinished(directory, deviceNode, results);
        }, Qt::QueuedConnection);
    });
}

} // namespace FlashSentry
