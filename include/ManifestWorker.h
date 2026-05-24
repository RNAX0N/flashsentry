#pragma once

#include "Types.h"

#include <QObject>
#include <QString>
#include <QMutex>
#include <QHash>
#include <memory>

namespace FlashSentry {

class ManifestWorker : public QObject {
    Q_OBJECT

public:
    enum class JobKind { Verify, BuildBaseline };

    struct Job {
        QString jobId;
        QString deviceNode;
        QString mountPoint;
        QString deviceId;
        JobKind kind = JobKind::Verify;
        WatchManifest manifest;
    };

    explicit ManifestWorker(QObject* parent = nullptr);

    QString startVerify(const QString& deviceNode, const QString& mountPoint,
                        const QString& deviceId, const WatchManifest& manifest);

    QString startBuildBaseline(const QString& deviceNode, const QString& mountPoint,
                               const QString& deviceId, const WatchManifest& spec);

    bool cancelJob(const QString& jobId);
    void cancelAll();

signals:
    void manifestStarted(const QString& jobId, const QString& deviceNode);
    void manifestCompleted(const QString& jobId, const ManifestVerifyResult& result);
    void manifestBaselineBuilt(const QString& jobId, const QString& deviceId, const WatchManifest& manifest);
    void manifestFailed(const QString& jobId, const QString& error);

private:
    struct JobState;
    QHash<QString, std::shared_ptr<JobState>> m_jobs;
    QMutex m_mutex;
};

} // namespace FlashSentry
