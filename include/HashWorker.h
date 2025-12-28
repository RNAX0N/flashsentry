#pragma once

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutex>
#include <QHash>
#include <QElapsedTimer>
#include <QTimer>
#include <atomic>
#include <memory>

#include "Types.h"

namespace FlashSentry {

/**
 * @brief HashWorker - High-performance asynchronous partition hashing
 * 
 * Uses QtConcurrent for true parallel execution without blocking the GUI.
 * Supports multiple hash algorithms, progress reporting, and cancellation.
 * Optimized for large partition hashing with memory-mapped I/O when available.
 */
class HashWorker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Supported hash algorithms
     */
    enum class Algorithm {
        SHA256,
        SHA512,
        BLAKE2b,
        XXH3_128  // Extremely fast, non-cryptographic
    };
    Q_ENUM(Algorithm)

    /**
     * @brief Hash job configuration
     */
    struct HashJob {
        QString deviceNode;
        Algorithm algorithm = Algorithm::SHA256;
        int bufferSizeKB = 1024;      // Read buffer size in KB
        bool useMemoryMapping = true;  // Use mmap when possible
        bool rawDevice = true;         // Hash raw device vs mounted files
        void* userData = nullptr;      // Optional user data
    };

    explicit HashWorker(QObject* parent = nullptr);
    ~HashWorker() override;

    // Prevent copying
    HashWorker(const HashWorker&) = delete;
    HashWorker& operator=(const HashWorker&) = delete;

    /**
     * @brief Start hashing a device asynchronously
     * @param job Hash job configuration
     * @return Job ID for tracking
     */
    QString startHash(const HashJob& job);

    /**
     * @brief Cancel a running hash job
     * @param jobId Job ID returned from startHash
     * @return true if job was found and cancelled
     */
    bool cancelHash(const QString& jobId);

    /**
     * @brief Cancel all running hash jobs
     */
    void cancelAll();

    /**
     * @brief Check if a specific job is running
     */
    bool isRunning(const QString& jobId) const;

    /**
     * @brief Check if any hash jobs are running
     */
    bool hasActiveJobs() const;

    /**
     * @brief Get the number of active hash jobs
     */
    int activeJobCount() const;

    /**
     * @brief Get progress for a specific job (0.0 - 1.0)
     */
    double progress(const QString& jobId) const;

    /**
     * @brief Set maximum concurrent hash operations
     */
    void setMaxConcurrent(int max);

    /**
     * @brief Get algorithm name as string
     */
    static QString algorithmName(Algorithm algo);

    /**
     * @brief Parse algorithm from string
     */
    static Algorithm algorithmFromName(const QString& name);

signals:
    /**
     * @brief Emitted when hashing starts for a job
     */
    void hashStarted(const QString& jobId, const QString& deviceNode);

    /**
     * @brief Emitted periodically during hashing
     * @param jobId Job identifier
     * @param progress 0.0 to 1.0
     * @param bytesProcessed Bytes hashed so far
     * @param speedMBps Current speed in MB/s
     */
    void hashProgress(const QString& jobId, double progress, 
                      quint64 bytesProcessed, double speedMBps);

    /**
     * @brief Emitted when hashing completes successfully
     */
    void hashCompleted(const QString& jobId, const FlashSentry::HashResult& result);

    /**
     * @brief Emitted when hashing fails
     */
    void hashFailed(const QString& jobId, const QString& error);

    /**
     * @brief Emitted when hashing is cancelled
     */
    void hashCancelled(const QString& jobId);

private:
    /**
     * @brief Internal job tracking structure
     */
    struct JobState {
        QString jobId;
        HashJob config;
        QFuture<HashResult> future;
        std::unique_ptr<QFutureWatcher<HashResult>> watcher;
        std::atomic<bool> cancelled{false};
        std::atomic<uint64_t> bytesProcessed{0};
        std::atomic<uint64_t> totalBytes{0};
        QElapsedTimer timer;
    };

    /**
     * @brief Execute hash operation (runs in thread pool)
     */
    static HashResult executeHash(std::shared_ptr<JobState> state);

    /**
     * @brief Hash using standard read() syscalls
     */
    static HashResult hashWithRead(std::shared_ptr<JobState> state);

    /**
     * @brief Hash using memory-mapped I/O
     */
    static HashResult hashWithMmap(std::shared_ptr<JobState> state);

    /**
     * @brief Get device size in bytes
     */
    static uint64_t getDeviceSize(const QString& deviceNode);

    /**
     * @brief Generate unique job ID
     */
    QString generateJobId();

    /**
     * @brief Handle job completion
     */
    void onJobFinished(const QString& jobId);

    /**
     * @brief Progress update timer callback
     */
    void updateProgress();

    // Active jobs
    mutable QMutex m_jobsMutex;
    QHash<QString, std::shared_ptr<JobState>> m_jobs;

    // Job ID counter
    mutable std::atomic<uint64_t> m_jobCounter{0};

    // Progress update timer
    QTimer* m_progressTimer = nullptr;

    // Configuration
    int m_maxConcurrent = 2;

    // Progress update interval in ms
    static constexpr int PROGRESS_UPDATE_INTERVAL_MS = 100;
};

} // namespace FlashSentry