#include "HashWorker.h"

#include <QTimer>
#include <QUuid>
#include <QDebug>
#include <QtConcurrent>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

namespace FlashSentry {

HashWorker::HashWorker(QObject* parent)
    : QObject(parent)
{
    // Create progress update timer
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
    state->totalBytes.store(getDeviceSize(job.deviceNode));
    state->timer.start();
    
    // Create future watcher
    state->watcher = std::make_unique<QFutureWatcher<HashResult>>();
    
    // Connect watcher signals
    connect(state->watcher.get(), &QFutureWatcher<HashResult>::finished,
            this, [this, jobId]() { onJobFinished(jobId); });
    
    // Start the hash operation in thread pool
    state->future = QtConcurrent::run(&HashWorker::executeHash, state);
    state->watcher->setFuture(state->future);
    
    {
        QMutexLocker locker(&m_jobsMutex);
        m_jobs.insert(jobId, state);
        
        // Start progress timer if not already running
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
    return Algorithm::SHA256;  // Default
}

QString HashWorker::generateJobId() const
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
        
        // Stop timer if no more jobs
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
        
        // Calculate speed
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
    // Choose hashing method based on configuration
    if (state->config.useMemoryMapping) {
        HashResult result = hashWithMmap(state);
        // Fall back to read() if mmap fails
        if (!result.success && result.errorMessage.contains("mmap")) {
            return hashWithRead(state);
        }
        return result;
    }
    
    return hashWithRead(state);
}

HashResult HashWorker::hashWithRead(std::shared_ptr<JobState> state)
{
    HashResult result;
    result.deviceNode = state->config.deviceNode;
    result.algorithm = algorithmName(state->config.algorithm);
    
    // Open device
    int fd = open(state->config.deviceNode.toStdString().c_str(), O_RDONLY | O_DIRECT);
    if (fd < 0) {
        // Try without O_DIRECT if it fails
        fd = open(state->config.deviceNode.toStdString().c_str(), O_RDONLY);
        if (fd < 0) {
            result.success = false;
            result.errorMessage = QString("Failed to open device: %1").arg(strerror(errno));
            return result;
        }
    }
    
    // Get device size
    uint64_t deviceSize = state->totalBytes.load();
    if (deviceSize == 0) {
        deviceSize = getDeviceSize(state->config.deviceNode);
        state->totalBytes.store(deviceSize);
    }
    
    // Initialize OpenSSL context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        close(fd);
        result.success = false;
        result.errorMessage = "Failed to create hash context";
        return result;
    }
    
    const EVP_MD* md = nullptr;
    switch (state->config.algorithm) {
        case Algorithm::SHA256:
            md = EVP_sha256();
            break;
        case Algorithm::SHA512:
            md = EVP_sha512();
            break;
        case Algorithm::BLAKE2b:
            md = EVP_blake2b512();
            break;
        case Algorithm::XXH3_128:
            // XXH3 not available in OpenSSL, fall back to SHA256
            md = EVP_sha256();
            result.algorithm = "SHA256";
            break;
    }
    
    if (EVP_DigestInit_ex(mdctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        close(fd);
        result.success = false;
        result.errorMessage = "Failed to initialize hash algorithm";
        return result;
    }
    
    // Allocate aligned buffer for O_DIRECT
    const size_t bufferSize = state->config.bufferSizeKB * 1024;
    void* buffer = nullptr;
    if (posix_memalign(&buffer, 4096, bufferSize) != 0) {
        EVP_MD_CTX_free(mdctx);
        close(fd);
        result.success = false;
        result.errorMessage = "Failed to allocate buffer";
        return result;
    }
    
    // Read and hash
    QElapsedTimer timer;
    timer.start();
    
    ssize_t bytesRead;
    uint64_t totalRead = 0;
    
    while ((bytesRead = read(fd, buffer, bufferSize)) > 0) {
        if (state->cancelled.load()) {
            free(buffer);
            EVP_MD_CTX_free(mdctx);
            close(fd);
            result.success = false;
            result.errorMessage = "Cancelled";
            return result;
        }
        
        EVP_DigestUpdate(mdctx, buffer, bytesRead);
        totalRead += bytesRead;
        state->bytesProcessed.store(totalRead);
    }
    
    if (bytesRead < 0) {
        free(buffer);
        EVP_MD_CTX_free(mdctx);
        close(fd);
        result.success = false;
        result.errorMessage = QString("Read error: %1").arg(strerror(errno));
        return result;
    }
    
    // Finalize hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);
    
    // Convert to hex string
    QByteArray hashHex;
    hashHex.reserve(hashLen * 2);
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QString("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }
    
    result.hash = QString::fromLatin1(hashHex);
    result.bytesProcessed = totalRead;
    result.durationMs = timer.elapsed();
    result.success = true;
    
    free(buffer);
    EVP_MD_CTX_free(mdctx);
    close(fd);
    
    return result;
}

HashResult HashWorker::hashWithMmap(std::shared_ptr<JobState> state)
{
    HashResult result;
    result.deviceNode = state->config.deviceNode;
    result.algorithm = algorithmName(state->config.algorithm);
    
    // Open device
    int fd = open(state->config.deviceNode.toStdString().c_str(), O_RDONLY);
    if (fd < 0) {
        result.success = false;
        result.errorMessage = QString("mmap: Failed to open device: %1").arg(strerror(errno));
        return result;
    }
    
    // Get device size
    uint64_t deviceSize = state->totalBytes.load();
    if (deviceSize == 0) {
        deviceSize = getDeviceSize(state->config.deviceNode);
        state->totalBytes.store(deviceSize);
    }
    
    if (deviceSize == 0) {
        close(fd);
        result.success = false;
        result.errorMessage = "mmap: Device size is 0";
        return result;
    }
    
    // Initialize OpenSSL context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        close(fd);
        result.success = false;
        result.errorMessage = "Failed to create hash context";
        return result;
    }
    
    const EVP_MD* md = nullptr;
    switch (state->config.algorithm) {
        case Algorithm::SHA256:
            md = EVP_sha256();
            break;
        case Algorithm::SHA512:
            md = EVP_sha512();
            break;
        case Algorithm::BLAKE2b:
            md = EVP_blake2b512();
            break;
        case Algorithm::XXH3_128:
            md = EVP_sha256();
            result.algorithm = "SHA256";
            break;
    }
    
    if (EVP_DigestInit_ex(mdctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        close(fd);
        result.success = false;
        result.errorMessage = "Failed to initialize hash algorithm";
        return result;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    // Map and hash in chunks to handle large devices
    const size_t chunkSize = 256 * 1024 * 1024;  // 256 MB chunks
    uint64_t offset = 0;
    
    while (offset < deviceSize) {
        if (state->cancelled.load()) {
            EVP_MD_CTX_free(mdctx);
            close(fd);
            result.success = false;
            result.errorMessage = "Cancelled";
            return result;
        }
        
        size_t mapSize = qMin(static_cast<uint64_t>(chunkSize), deviceSize - offset);
        
        void* mapped = mmap(nullptr, mapSize, PROT_READ, MAP_PRIVATE, fd, offset);
        if (mapped == MAP_FAILED) {
            EVP_MD_CTX_free(mdctx);
            close(fd);
            result.success = false;
            result.errorMessage = QString("mmap failed: %1").arg(strerror(errno));
            return result;
        }
        
        // Advise kernel of sequential access
        madvise(mapped, mapSize, MADV_SEQUENTIAL);
        
        // Hash this chunk
        EVP_DigestUpdate(mdctx, mapped, mapSize);
        
        munmap(mapped, mapSize);
        
        offset += mapSize;
        state->bytesProcessed.store(offset);
    }
    
    // Finalize hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestFinal_ex(mdctx, hash, &hashLen);
    
    // Convert to hex string
    QByteArray hashHex;
    hashHex.reserve(hashLen * 2);
    for (unsigned int i = 0; i < hashLen; ++i) {
        hashHex.append(QString("%1").arg(hash[i], 2, 16, QChar('0')).toLatin1());
    }
    
    result.hash = QString::fromLatin1(hashHex);
    result.bytesProcessed = offset;
    result.durationMs = timer.elapsed();
    result.success = true;
    
    EVP_MD_CTX_free(mdctx);
    close(fd);
    
    return result;
}

uint64_t HashWorker::getDeviceSize(const QString& deviceNode)
{
    int fd = open(deviceNode.toStdString().c_str(), O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    
    uint64_t size = 0;
    
    // Try BLKGETSIZE64 ioctl first (works for block devices)
    if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
        close(fd);
        return size;
    }
    
    // Fall back to lseek
    off_t end = lseek(fd, 0, SEEK_END);
    if (end > 0) {
        size = static_cast<uint64_t>(end);
    }
    
    close(fd);
    return size;
}

} // namespace FlashSentry