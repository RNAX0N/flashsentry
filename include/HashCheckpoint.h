#pragma once

#include "Types.h"

#include <optional>
#include <QList>
#include <QString>
#include <QStringList>

namespace FlashSentry {

struct HashCheckpoint {
    QString deviceNode;
    QString algorithm;
    QString scanMode;
    uint64_t deviceSize = 0;
    uint64_t blockSize = 0;
    QStringList blockHashes;
    uint64_t bytesCompleted = 0;

    bool isValid() const { return !deviceNode.isEmpty() && blockSize > 0; }
};

class HashCheckpointStore {
public:
    static HashCheckpointStore& instance();

    void load();
    void save();

    std::optional<HashCheckpoint> checkpointFor(const QString& deviceNode,
                                                const QString& algorithm,
                                                const QString& scanMode) const;

    void upsert(const HashCheckpoint& cp);
    void remove(const QString& deviceNode, const QString& algorithm, const QString& scanMode);
    void clearAll();

private:
    HashCheckpointStore() = default;

    QList<HashCheckpoint> m_checkpoints;
};

} // namespace FlashSentry
