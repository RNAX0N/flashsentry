#include "HashWorker.h"
#include "RawDeviceHash.h"

#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QtConcurrent>

namespace FlashSentry {

namespace {

RawDeviceHash::Algorithm toRawAlgorithm(HashWorker::Algorithm algo)
{
    switch (algo) {
        case HashWorker::Algorithm::SHA512: return RawDeviceHash::Algorithm::SHA512;
        case HashWorker::Algorithm::BLAKE2b: return RawDeviceHash::Algorithm::BLAKE2b;
        case HashWorker::Algorithm::SHA256:
        case HashWorker::Algorithm::XXH3_128:
        default:
            return RawDeviceHash::Algorithm::SHA256;
    }
}

} // namespace

HashWorker::HashWorker(QObject* parent)
    : QObject(parent)
{
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(PROGRESS_UPDATE_INTERVAL_MS);
    connect(m_progressTimer, &QTimer::timeout, this, &HashWorker::updateProgress);
}

HashWorker::~HashWorker()
{
    cancelAll();
}

QString HashWorker::startHash(const HashJob& job)
{
    QString jobId = generateJobId();

    auto state = std::make_shared<JobState>();
    state->jobId = jobId;
    state->config = job;
    state->cancelled.store(false);
    state->bytesProcessed.store(0);

    const int fd = RawDeviceHash::openDevice(job.deviceNode);
    if (fd >= 0) {
        state->totalBytes.store(RawDeviceHash::deviceSize(fd, job.deviceNode));
        RawDeviceHash::closeDevice(fd);
    } else {
        state->totalBytes.store(0);
    }

    state->watcher = std::make_unique<QFutureWatcher<HashResult>>();
    connect(state->watcher.get(), &QFutureWatcher<HashResult>::finished,
            this, [this, jobId]() { onJobFinished(jobId); });

    state->future = QtConcurrent::run(&HashWorker::executeHash, state);
    state->watcher->setFuture(state->future);

    {
        QMutexLocker locker(&m_jobsMutex);
        m_jobs.insert(jobId, state);

        if (!m_progressTimer->isActive()) {
            m_progressTimer->start();
        }
    }

    emit hashStarted(jobId, job.deviceNode);
    return jobId;
}

bool HashWorker::cancelHash(const QString& jobId)
{
    QMutexLocker locker(&m_jobsMutex);

    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return false;
    }

    (*it)->cancelled.store(true);
    return true;
}

void HashWorker::cancelAll()
{
    QMutexLocker locker(&m_jobsMutex);

    for (auto& state : m_jobs) {
        state->cancelled.store(true);
    }
}

bool HashWorker::isRunning(const QString& jobId) const
{
    QMutexLocker locker(&m_jobsMutex);
    return m_jobs.contains(jobId);
}

bool HashWorker::hasActiveJobs() const
{
    QMutexLocker locker(&m_jobsMutex);
    return !m_jobs.isEmpty();
}

int HashWorker::activeJobCount() const
{
    QMutexLocker locker(&m_jobsMutex);
    return m_jobs.size();
}

double HashWorker::progress(const QString& jobId) const
{
    QMutexLocker locker(&m_jobsMutex);

    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return 0.0;
    }

    uint64_t total = (*it)->totalBytes.load();
    if (total == 0) return 0.0;

    return static_cast<double>((*it)->bytesProcessed.load()) / static_cast<double>(total);
}

void HashWorker::setMaxConcurrent(int max)
{
    m_maxConcurrent = qMax(1, max);
}

QString HashWorker::algorithmName(Algorithm algo)
{
    switch (algo) {
        case Algorithm::SHA256:   return "SHA256";
        case Algorithm::SHA512:   return "SHA512";
        case Algorithm::BLAKE2b:  return "BLAKE2b";
        case Algorithm::XXH3_128: return "XXH3-128";
    }
    return "Unknown";
}

HashWorker::Algorithm HashWorker::algorithmFromName(const QString& name)
{
    if (name.compare("SHA256", Qt::CaseInsensitive) == 0) return Algorithm::SHA256;
    if (name.compare("SHA512", Qt::CaseInsensitive) == 0) return Algorithm::SHA512;
    if (name.compare("BLAKE2b", Qt::CaseInsensitive) == 0) return Algorithm::BLAKE2b;
    if (name.compare("XXH3-128", Qt::CaseInsensitive) == 0) return Algorithm::XXH3_128;
    return Algorithm::SHA256;
}

QString HashWorker::generateJobId()
{
    return QString("hash_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_jobCounter.fetch_add(1));
}

void HashWorker::onJobFinished(const QString& jobId)
{
    std::shared_ptr<JobState> state;

    {
        QMutexLocker locker(&m_jobsMutex);
        auto it = m_jobs.find(jobId);
        if (it == m_jobs.end()) {
            return;
        }
        state = *it;
        m_jobs.erase(it);

        if (m_jobs.isEmpty()) {
            m_progressTimer->stop();
        }
    }

    if (state->cancelled.load()) {
        emit hashCancelled(jobId);
        return;
    }

    try {
        HashResult result = state->future.result();

        if (result.success) {
            emit hashCompleted(jobId, result);
        } else {
            emit hashFailed(jobId, result.errorMessage);
        }
    } catch (const std::exception& e) {
        emit hashFailed(jobId, QString("Exception: %1").arg(e.what()));
    }
}

void HashWorker::updateProgress()
{
    QMutexLocker locker(&m_jobsMutex);

    for (auto& state : m_jobs) {
        uint64_t total = state->totalBytes.load();
        uint64_t processed = state->bytesProcessed.load();

        if (total == 0) continue;

        double prog = static_cast<double>(processed) / static_cast<double>(total);

        qint64 elapsedMs = state->timer.elapsed();
        double speedMBps = 0.0;
        if (elapsedMs > 0) {
            speedMBps = (static_cast<double>(processed) / (1024.0 * 1024.0)) /
                        (static_cast<double>(elapsedMs) / 1000.0);
        }

        emit hashProgress(state->jobId, prog, processed, speedMBps);
    }
}

HashResult HashWorker::executeHash(std::shared_ptr<JobState> state)
{
    RawDeviceHash::Options options;
    options.deviceNode = state->config.deviceNode;
    options.algorithm = toRawAlgorithm(state->config.algorithm);
    options.bufferSizeKB = state->config.bufferSizeKB;
    options.useMemoryMapping = state->config.useMemoryMapping;
    options.cancelled = &state->cancelled;
    options.bytesProcessed = &state->bytesProcessed;

    if (state->totalBytes.load() == 0) {
        const int fd = RawDeviceHash::openDevice(state->config.deviceNode);
        if (fd >= 0) {
            state->totalBytes.store(RawDeviceHash::deviceSize(fd, state->config.deviceNode));
            RawDeviceHash::closeDevice(fd);
        }
    }

    QElapsedTimer timer;
    timer.start();

    HashResult result = RawDeviceHash::hashDevice(options);
    result.durationMs = static_cast<uint64_t>(timer.elapsed());

    if (state->config.algorithm == Algorithm::XXH3_128 && result.success) {
        result.algorithm = "SHA256";
    }

    return result;
}

uint64_t HashWorker::getDeviceSize(const QString& deviceNode)
{
    const int fd = RawDeviceHash::openDevice(deviceNode);
    if (fd < 0) {
        return 0;
    }
    const uint64_t size = RawDeviceHash::deviceSize(fd, deviceNode);
    RawDeviceHash::closeDevice(fd);
    return size;
}

HashResult HashWorker::hashWithRead(std::shared_ptr<JobState> state)
{
    Q_UNUSED(state)
    HashResult r;
    r.success = false;
    r.errorMessage = "Internal error: hashWithRead is deprecated";
    return r;
}

HashResult HashWorker::hashWithMmap(std::shared_ptr<JobState> state)
{
    Q_UNUSED(state)
    HashResult r;
    r.success = false;
    r.errorMessage = "Internal error: hashWithMmap is deprecated";
    return r;
}

} // namespace FlashSentry
