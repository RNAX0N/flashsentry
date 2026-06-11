#include "HashWorker.h"
#include "HashCheckpoint.h"
#include "RawDeviceHash.h"
#include "RawDeviceHashAdvanced.h"

#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QtConcurrent>

namespace FlashSpartan {

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

RawDeviceHash::ScanMode toRawScanMode(HashScanMode mode)
{
    return mode == HashScanMode::QuickSample ? RawDeviceHash::ScanMode::QuickSample
                                             : RawDeviceHash::ScanMode::Full;
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
    const QString jobId = generateJobId();

    auto state = std::make_shared<JobState>();
    state->jobId = jobId;
    state->config = job;
    state->cancelled.store(false);
    state->bytesProcessed.store(0);

    const int fd = RawDeviceHash::openDevice(job.deviceNode);
    if (fd >= 0) {
        state->totalBytes.store(RawDeviceHash::deviceSize(fd, job.deviceNode));
        RawDeviceHash::closeDevice(fd);
    }

    {
        QMutexLocker locker(&m_jobsMutex);
        if (static_cast<int>(m_jobs.size()) >= m_maxConcurrent) {
            m_pendingQueue.enqueue(qMakePair(jobId, job));
            return jobId;
        }
    }

    return launchJob(jobId, job, std::move(state));
}

QString HashWorker::launchJob(const QString& jobId, const HashJob& job,
                              std::shared_ptr<JobState> state)
{
    if (!state) {
        state = std::make_shared<JobState>();
        state->jobId = jobId;
        state->config = job;
        state->cancelled.store(false);
        state->bytesProcessed.store(0);
        const int fd = RawDeviceHash::openDevice(job.deviceNode);
        if (fd >= 0) {
            state->totalBytes.store(RawDeviceHash::deviceSize(fd, job.deviceNode));
            RawDeviceHash::closeDevice(fd);
        }
    }

    state->timer.start();
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
        for (int i = 0; i < m_pendingQueue.size(); ++i) {
            if (m_pendingQueue.at(i).first == jobId) {
                m_pendingQueue.removeAt(i);
                return true;
            }
        }
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
    m_pendingQueue.clear();
}

bool HashWorker::isRunning(const QString& jobId) const
{
    QMutexLocker locker(&m_jobsMutex);
    return m_jobs.contains(jobId);
}

bool HashWorker::hasActiveJobs() const
{
    QMutexLocker locker(&m_jobsMutex);
    return !m_jobs.isEmpty() || !m_pendingQueue.isEmpty();
}

int HashWorker::activeJobCount() const
{
    QMutexLocker locker(&m_jobsMutex);
    return m_jobs.size() + m_pendingQueue.size();
}

double HashWorker::progress(const QString& jobId) const
{
    QMutexLocker locker(&m_jobsMutex);

    auto it = m_jobs.find(jobId);
    if (it == m_jobs.end()) {
        return 0.0;
    }

    const uint64_t total = (*it)->totalBytes.load();
    if (total == 0) {
        return 0.0;
    }

    return static_cast<double>((*it)->bytesProcessed.load()) / static_cast<double>(total);
}

void HashWorker::setMaxConcurrent(int max)
{
    m_maxConcurrent = qMax(1, max);
}

QString HashWorker::algorithmName(Algorithm algo)
{
    switch (algo) {
        case Algorithm::SHA256: return "SHA256";
        case Algorithm::SHA512: return "SHA512";
        case Algorithm::BLAKE2b: return "BLAKE2b";
        case Algorithm::XXH3_128: return "XXH3-128";
    }
    return "Unknown";
}

HashWorker::Algorithm HashWorker::algorithmFromName(const QString& name)
{
    const QString base = name.section(QLatin1Char('-'), 0, 0);
    if (base.compare("SHA256", Qt::CaseInsensitive) == 0) return Algorithm::SHA256;
    if (base.compare("SHA512", Qt::CaseInsensitive) == 0) return Algorithm::SHA512;
    if (base.compare("BLAKE2b", Qt::CaseInsensitive) == 0) return Algorithm::BLAKE2b;
    if (base.compare("XXH3-128", Qt::CaseInsensitive) == 0) return Algorithm::XXH3_128;
    return Algorithm::SHA256;
}

QString HashWorker::generateJobId()
{
    return QString("hash_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_jobCounter.fetch_add(1));
}

void HashWorker::processPendingQueue()
{
    for (;;) {
        QPair<QString, HashJob> pair;
        {
            QMutexLocker locker(&m_jobsMutex);
            if (m_jobs.size() >= static_cast<size_t>(m_maxConcurrent) || m_pendingQueue.isEmpty()) {
                break;
            }
            pair = m_pendingQueue.dequeue();
        }
        launchJob(pair.first, pair.second, nullptr);
    }
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

        if (m_jobs.isEmpty() && m_pendingQueue.isEmpty()) {
            m_progressTimer->stop();
        }
    }

    processPendingQueue();

    if (state->cancelled.load()) {
        emit hashCancelled(jobId);
        return;
    }

    try {
        HashResult result = state->future.result();

        if (result.success) {
            result.hashScopeLabel = state->config.scope == HashScope::WholeDisk
                                        ? QStringLiteral("whole_disk")
                                        : QStringLiteral("partition");
            result.scanModeLabel = hashScanModeToString(state->config.scanMode);
            result.resumedFromCheckpoint = state->config.resumeFromCheckpoint;
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
        const uint64_t total = state->totalBytes.load();
        const uint64_t processed = state->bytesProcessed.load();

        if (total == 0) {
            continue;
        }

        const double prog = static_cast<double>(processed) / static_cast<double>(total);

        const qint64 elapsedMs = state->timer.elapsed();
        double speedMBps = 0.0;
        if (elapsedMs > 100) {
            speedMBps = (static_cast<double>(processed) / (1024.0 * 1024.0))
                / (static_cast<double>(elapsedMs) / 1000.0);
        }

        double etaSeconds = 0.0;
        if (speedMBps > 0.01 && processed < total) {
            const double remainingMb =
                static_cast<double>(total - processed) / (1024.0 * 1024.0);
            etaSeconds = remainingMb / speedMBps;
        }

        emit hashProgress(state->jobId, prog, processed, speedMBps, etaSeconds, total);
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
    options.scanMode = toRawScanMode(state->config.scanMode);

    HashCheckpoint checkpoint;
    HashCheckpoint* cpPtr = nullptr;
    if (state->config.scanMode == HashScanMode::Full) {
        const QString algo = algorithmName(state->config.algorithm);
        if (auto existing = HashCheckpointStore::instance().checkpointFor(
                state->config.deviceNode, algo, QStringLiteral("full"))) {
            checkpoint = *existing;
        }
        cpPtr = &checkpoint;
        options.checkpointOut = cpPtr;
        if (checkpoint.isValid()) {
            options.resumeFromBytes = checkpoint.bytesCompleted;
        }
    }

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

    if (result.success && cpPtr && cpPtr->isValid() && !result.errorMessage.contains(
            QStringLiteral("Cancelled"))) {
        HashCheckpointStore::instance().remove(state->config.deviceNode, cpPtr->algorithm,
                                               cpPtr->scanMode);
    } else if (!result.success && result.errorMessage.contains(QStringLiteral("Cancelled"))
               && cpPtr && cpPtr->isValid()) {
        HashCheckpointStore::instance().upsert(*cpPtr);
    }

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

} // namespace FlashSpartan
