#include "ManifestWorker.h"
#include "ManifestService.h"

#include <QUuid>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMutexLocker>

namespace FlashSentry {

struct ManifestWorker::JobState {
    Job config;
    std::unique_ptr<QFutureWatcher<ManifestVerifyResult>> verifyWatcher;
    std::unique_ptr<QFutureWatcher<WatchManifest>> buildWatcher;
};

ManifestWorker::ManifestWorker(QObject* parent)
    : QObject(parent)
{
}

QString ManifestWorker::startVerify(const QString& deviceNode, const QString& mountPoint,
                                    const QString& deviceId, const WatchManifest& manifest)
{
    Job job;
    job.jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job.deviceNode = deviceNode;
    job.mountPoint = mountPoint;
    job.deviceId = deviceId;
    job.kind = JobKind::Verify;
    job.manifest = manifest;

    auto state = std::make_shared<JobState>();
    state->config = job;
    state->verifyWatcher = std::make_unique<QFutureWatcher<ManifestVerifyResult>>();

    connect(state->verifyWatcher.get(), &QFutureWatcher<ManifestVerifyResult>::finished, this,
            [this, jobId = job.jobId]() {
                std::shared_ptr<JobState> st;
                {
                    QMutexLocker lock(&m_mutex);
                    auto it = m_jobs.find(jobId);
                    if (it == m_jobs.end()) {
                        return;
                    }
                    st = it.value();
                }
                ManifestVerifyResult result = st->verifyWatcher->result();
                result.deviceNode = st->config.deviceNode;
                {
                    QMutexLocker lock(&m_mutex);
                    m_jobs.remove(jobId);
                }
                if (!result.success) {
                    emit manifestFailed(jobId, result.errorMessage);
                } else {
                    emit manifestCompleted(jobId, result);
                }
            });

    const QFuture<ManifestVerifyResult> future = QtConcurrent::run(
        [mountPoint, manifest]() {
            auto vr = ManifestService::verifyManifest(mountPoint, manifest);
            ManifestVerifyResult r;
            r.success = vr.success;
            r.matches = vr.matches;
            r.computedRootHex = vr.computedRootHex;
            r.expectedRootHex = vr.expectedRootHex;
            r.changedPaths = vr.changedPaths;
            r.missingPaths = vr.missingPaths;
            r.addedPaths = vr.addedPaths;
            r.errorMessage = vr.errorMessage;
            r.filesChecked = vr.filesChecked;
            r.durationMs = vr.durationMs;
            r.success = r.errorMessage.isEmpty() || r.filesChecked > 0 || !manifest.groups.isEmpty();
            if (manifest.groups.isEmpty()) {
                r.success = false;
                r.errorMessage = QStringLiteral("No watch groups configured");
            }
            return r;
        });

    state->verifyWatcher->setFuture(future);
    {
        QMutexLocker lock(&m_mutex);
        m_jobs.insert(job.jobId, state);
    }
    emit manifestStarted(job.jobId, deviceNode);
    return job.jobId;
}

QString ManifestWorker::startBuildBaseline(const QString& deviceNode, const QString& mountPoint,
                                           const QString& deviceId, const WatchManifest& spec)
{
    Job job;
    job.jobId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job.deviceNode = deviceNode;
    job.mountPoint = mountPoint;
    job.deviceId = deviceId;
    job.kind = JobKind::BuildBaseline;
    job.manifest = spec;

    auto state = std::make_shared<JobState>();
    state->config = job;
    state->buildWatcher = std::make_unique<QFutureWatcher<WatchManifest>>();

    connect(state->buildWatcher.get(), &QFutureWatcher<WatchManifest>::finished, this,
            [this, jobId = job.jobId]() {
                std::shared_ptr<JobState> st;
                {
                    QMutexLocker lock(&m_mutex);
                    auto it = m_jobs.find(jobId);
                    if (it == m_jobs.end()) {
                        return;
                    }
                    st = it.value();
                }
                const WatchManifest built = st->buildWatcher->result();
                {
                    QMutexLocker lock(&m_mutex);
                    m_jobs.remove(jobId);
                }
                emit manifestBaselineBuilt(jobId, st->config.deviceId, built);
            });

    const QFuture<WatchManifest> future = QtConcurrent::run([mountPoint, spec]() {
        return ManifestService::rebuildManifestRoots(mountPoint, spec);
    });

    state->buildWatcher->setFuture(future);
    {
        QMutexLocker lock(&m_mutex);
        m_jobs.insert(job.jobId, state);
    }
    emit manifestStarted(job.jobId, deviceNode);
    return job.jobId;
}

bool ManifestWorker::cancelJob(const QString& jobId)
{
    QMutexLocker lock(&m_mutex);
    return m_jobs.remove(jobId);
}

void ManifestWorker::cancelAll()
{
    QMutexLocker lock(&m_mutex);
    m_jobs.clear();
}

} // namespace FlashSentry
