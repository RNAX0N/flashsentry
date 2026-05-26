#pragma once

#include "Types.h"

#include <QObject>
#include <QHash>
#include <QMutex>
#include <optional>

namespace FlashSentry {

class BadUsbBaselineStore : public QObject {
    Q_OBJECT

public:
    explicit BadUsbBaselineStore(QObject* parent = nullptr);

    bool initialize(const QString& path = QString());
    QString baselinePath() const;

    QList<BadUsbBaselineEntry> allDevices() const;
    bool hasDevice(const QString& stableId) const;
    std::optional<BadUsbBaselineEntry> getDevice(const QString& stableId) const;
    std::optional<BadUsbBaselineEntry> matchDevice(const HidDeviceInfo& info) const;

    static QStringList identifierAliasesFor(const HidDeviceInfo& info);

    bool upsertBaseline(const HidDeviceInfo& info, bool trusted = true,
                        const QString& notes = {},
                        HidDeviceCategory category = HidDeviceCategory::Unknown);
    bool setTrusted(const QString& stableId, bool trusted);
    bool removeDevice(const QString& stableId);
    bool save() const;
    bool load();

signals:
    void baselineChanged();

private:
    QString defaultPath() const;

    mutable QMutex m_mutex;
    QString m_path;
    QHash<QString, BadUsbBaselineEntry> m_devices;
};

} // namespace FlashSentry
