#include "BadUsbBaselineStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

namespace FlashSpartan {

BadUsbBaselineStore::BadUsbBaselineStore(QObject* parent)
    : QObject(parent)
{
}

QString BadUsbBaselineStore::defaultPath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/badusb-baseline.json");
}

bool BadUsbBaselineStore::initialize(const QString& path)
{
    {
        QMutexLocker locker(&m_mutex);
        m_path = path.isEmpty() ? defaultPath() : path;
    }
    return load();
}

QString BadUsbBaselineStore::baselinePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_path;
}

QList<BadUsbBaselineEntry> BadUsbBaselineStore::allDevices() const
{
    QMutexLocker locker(&m_mutex);
    return m_devices.values();
}

bool BadUsbBaselineStore::hasDevice(const QString& stableId) const
{
    QMutexLocker locker(&m_mutex);
    return m_devices.contains(stableId);
}

std::optional<BadUsbBaselineEntry> BadUsbBaselineStore::getDevice(const QString& stableId) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_devices.constFind(stableId);
    if (it == m_devices.constEnd()) {
        return std::nullopt;
    }
    return *it;
}

bool BadUsbBaselineStore::upsertBaseline(const HidDeviceInfo& info, bool trusted, const QString& notes)
{
    const QString stableId = info.stableId();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    {
        QMutexLocker locker(&m_mutex);
        BadUsbBaselineEntry entry = m_devices.value(stableId);
        if (entry.stableId.isEmpty()) {
            entry.stableId = stableId;
            entry.firstSeenUtc = now;
        }
        entry.device = info;
        entry.trusted = trusted;
        entry.lastSeenUtc = now;
        if (!notes.isEmpty()) {
            entry.notes = notes;
        }
        m_devices.insert(stableId, entry);
    }
    const bool ok = save();
    if (ok) {
        emit baselineChanged();
    }
    return ok;
}

bool BadUsbBaselineStore::setTrusted(const QString& stableId, bool trusted)
{
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_devices.find(stableId);
        if (it == m_devices.end()) {
            return false;
        }
        it->trusted = trusted;
        it->lastSeenUtc = QDateTime::currentDateTimeUtc();
    }
    const bool ok = save();
    if (ok) {
        emit baselineChanged();
    }
    return ok;
}

bool BadUsbBaselineStore::removeDevice(const QString& stableId)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_devices.remove(stableId) == 0) {
            return false;
        }
    }
    const bool ok = save();
    if (ok) {
        emit baselineChanged();
    }
    return ok;
}

bool BadUsbBaselineStore::load()
{
    QString path;
    {
        QMutexLocker locker(&m_mutex);
        if (m_path.isEmpty()) {
            m_path = defaultPath();
        }
        path = m_path;
        m_devices.clear();
    }

    QFile file(path);
    if (!file.exists()) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QHash<QString, BadUsbBaselineEntry> loaded;
    const QJsonArray devices = doc.object().value(QStringLiteral("devices")).toArray();
    for (const QJsonValue& value : devices) {
        const BadUsbBaselineEntry entry = BadUsbBaselineEntry::fromJson(value.toObject());
        if (!entry.stableId.isEmpty()) {
            loaded.insert(entry.stableId, entry);
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_devices = loaded;
    }
    return true;
}

bool BadUsbBaselineStore::save() const
{
    QString path;
    QList<BadUsbBaselineEntry> entries;
    {
        QMutexLocker locker(&m_mutex);
        path = m_path.isEmpty() ? defaultPath() : m_path;
        entries = m_devices.values();
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject root;
    root.insert(QStringLiteral("version"), QStringLiteral("1.0"));
    QJsonArray devices;
    for (const BadUsbBaselineEntry& entry : entries) {
        devices.append(entry.toJson());
    }
    root.insert(QStringLiteral("devices"), devices);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return false;
    }
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

} // namespace FlashSpartan
