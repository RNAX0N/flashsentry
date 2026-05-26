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
    m_cancelledFlag = true;
}

void IsoVerifierWorker::verifyMountPoint(const QString& mountPoint, const QString& deviceNode)
{
    m_cancelled = false;
    m_cancelledFlag = false;
    IsoVerifyOptions opt = IsoVerifier::verifyOptions();
    opt.cancelled = &m_cancelledFlag;
    opt.progress = [this](int current, int total, const QString& file) {
        emit verificationProgress(
            QStringLiteral("Verifying %1 (%2/%3)…").arg(file).arg(current).arg(total));
    };
    IsoVerifier::setVerifyOptions(opt);

    emit verificationStarted(mountPoint, deviceNode);

    m_activeJob = QtConcurrent::run([this, mountPoint, deviceNode]() {
        if (m_cancelled) return;
        emit verificationProgress(QStringLiteral("Scanning %1 for images…").arg(mountPoint));

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
    m_cancelledFlag = false;
    IsoVerifyOptions opt = IsoVerifier::verifyOptions();
    opt.cancelled = &m_cancelledFlag;
    opt.progress = [this](int current, int total, const QString& file) {
        emit verificationProgress(
            QStringLiteral("Verifying %1 (%2/%3)…").arg(file).arg(current).arg(total));
    };
    IsoVerifier::setVerifyOptions(opt);

    emit verificationStarted(directory, deviceNode);
    emit verificationProgress(QStringLiteral("Verifying images in %1…").arg(directory));

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
