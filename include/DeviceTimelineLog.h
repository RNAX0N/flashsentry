#pragma once

#include "UiEventTypes.h"

#include <QDateTime>
#include <QList>
#include <QString>

namespace FlashSpartan {

/** Persistent per-device timeline (connect, verify, etc.). */
class DeviceTimelineLog {
public:
    static DeviceTimelineLog& instance();

    void load();
    void save();

    void append(const UiEventEntry& entry);

    QList<UiEventEntry> entriesForDevice(const QString& deviceNode, int retentionDays,
                                        int maxEntries) const;

    QStringList knownDeviceNodes() const;

private:
    DeviceTimelineLog() = default;

    QList<UiEventEntry> m_entries;
    static constexpr int kMaxTotalEntries = 10000;
};

} // namespace FlashSpartan
