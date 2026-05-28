#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace FlashSentry {

enum class VerifyHistoryKind {
    Hash,
    Manifest,
    IsoScan,
};

/** One completed verification event (persisted in verify-history.json). */
struct VerifyHistoryEntry {
    QDateTime timestamp;
    QString deviceNode;
    QString deviceLabel;
    QString mountPoint;
    VerifyHistoryKind kind = VerifyHistoryKind::Hash;
  /** pass | fail | mismatch | error | partial */
    QString status;
    QString summary;
    QString detail;
    uint64_t durationMs = 0;
};

class VerifyHistory {
public:
    static VerifyHistory& instance();

    void load();
    void save();

    void append(const VerifyHistoryEntry& entry);

    QList<VerifyHistoryEntry> recentEntries(int limit = 50) const;
    QList<VerifyHistoryEntry> entriesForDevice(const QString& deviceNode, int limit = 20) const;

    QString formatEntryLine(const VerifyHistoryEntry& entry) const;

private:
    VerifyHistory() = default;

    QList<VerifyHistoryEntry> m_entries;
    static constexpr int kMaxEntries = 500;
};

} // namespace FlashSentry
