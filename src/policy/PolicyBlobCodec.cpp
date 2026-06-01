#include "policy/PolicyBlobCodec.h"

#include "policy/PolicyPaths.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace FlashSpartan::Policy {

namespace {

QByteArray writeString(QDataStream& out, const QString& s)
{
    const QByteArray utf = s.toUtf8();
    out << quint32(utf.size());
    if (!utf.isEmpty()) {
        out.writeRawData(utf.constData(), utf.size());
    }
    return {};
}

bool readString(QDataStream& in, QString& s)
{
    quint32 len = 0;
    in >> len;
    if (in.status() != QDataStream::Ok || len > 16 * 1024 * 1024) {
        return false;
    }
    QByteArray buf;
    buf.resize(static_cast<int>(len));
    if (len > 0) {
        if (in.readRawData(buf.data(), static_cast<int>(len)) != static_cast<int>(len)) {
            return false;
        }
    }
    s = QString::fromUtf8(buf);
    return in.status() == QDataStream::Ok;
}

QByteArray serializeDevice(const DeviceRecord& rec)
{
    return QJsonDocument(rec.toJson()).toJson(QJsonDocument::Compact);
}

DeviceRecord deserializeDevice(const QByteArray& blob)
{
    const QJsonDocument doc = QJsonDocument::fromJson(blob);
    if (!doc.isObject()) {
        return {};
    }
    return DeviceRecord::fromJson(doc.object());
}

QByteArray serializeBlock(const BlockedDriveEntry& e)
{
    QJsonObject o;
    o[QStringLiteral("drive_key")] = e.driveKey;
    o[QStringLiteral("unique_id")] = e.uniqueId;
    o[QStringLiteral("label")] = e.label;
    o[QStringLiteral("blocked_at")] = e.blockedAt.toString(Qt::ISODate);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

BlockedDriveEntry deserializeBlock(const QByteArray& blob)
{
    const QJsonObject o = QJsonDocument::fromJson(blob).object();
    BlockedDriveEntry e;
    e.driveKey = o[QStringLiteral("drive_key")].toString();
    e.uniqueId = o[QStringLiteral("unique_id")].toString();
    e.label = o[QStringLiteral("label")].toString();
    e.blockedAt = QDateTime::fromString(o[QStringLiteral("blocked_at")].toString(), Qt::ISODate);
    return e;
}

} // namespace

QByteArray PolicyBlobCodec::encode(const PolicySnapshot& snapshot)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_4);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint32(snapshot.devices.size());
    for (const DeviceRecord& rec : snapshot.devices) {
        const QByteArray blob = serializeDevice(rec);
        out << quint32(blob.size());
        out.writeRawData(blob.constData(), blob.size());
    }

    out << quint32(snapshot.blocks.size());
    for (const BlockedDriveEntry& e : snapshot.blocks) {
        const QByteArray blob = serializeBlock(e);
        out << quint32(blob.size());
        out.writeRawData(blob.constData(), blob.size());
    }

    return payload;
}

bool PolicyBlobCodec::decode(const QByteArray& fileBytes, PolicySnapshot& out, QString* error)
{
    if (fileBytes.size() < 10 + 32) {
        if (error) {
            *error = QStringLiteral("Policy store file too small");
        }
        return false;
    }

    QDataStream header(fileBytes);
    header.setByteOrder(QDataStream::LittleEndian);

    quint32 magicRead = 0;
    quint16 version = 0;
    quint32 payloadLen = 0;
    header >> magicRead >> version >> payloadLen;
    if (magicRead != kMagic || version != kVersion) {
        if (error) {
            *error = QStringLiteral("Invalid policy store header");
        }
        return false;
    }
    if (fileBytes.size() < 10 + static_cast<int>(payloadLen) + 32) {
        if (error) {
            *error = QStringLiteral("Truncated policy store");
        }
        return false;
    }

    const QByteArray payload = fileBytes.mid(10, static_cast<int>(payloadLen));
    const QByteArray sig = fileBytes.mid(10 + static_cast<int>(payloadLen), 32);

    const QByteArray key = loadOrCreateKey();
    if (!verify(payload, sig, key)) {
        if (error) {
            *error = QStringLiteral("Policy store integrity check failed (HMAC)");
        }
        return false;
    }

    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_4);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 deviceCount = 0;
    in >> deviceCount;
    out.devices.clear();
    for (quint32 i = 0; i < deviceCount; ++i) {
        quint32 blobLen = 0;
        in >> blobLen;
        if (blobLen > 8 * 1024 * 1024) {
            if (error) {
                *error = QStringLiteral("Corrupt device record");
            }
            return false;
        }
        QByteArray blob;
        blob.resize(static_cast<int>(blobLen));
        if (in.readRawData(blob.data(), static_cast<int>(blobLen)) != static_cast<int>(blobLen)) {
            if (error) {
                *error = QStringLiteral("Unexpected end of device data");
            }
            return false;
        }
        DeviceRecord rec = deserializeDevice(blob);
        if (!rec.uniqueId.isEmpty()) {
            out.devices.append(rec);
        }
    }

    quint32 blockCount = 0;
    in >> blockCount;
    out.blocks.clear();
    for (quint32 i = 0; i < blockCount; ++i) {
        quint32 blobLen = 0;
        in >> blobLen;
        QByteArray blob;
        blob.resize(static_cast<int>(blobLen));
        if (in.readRawData(blob.data(), static_cast<int>(blobLen)) != static_cast<int>(blobLen)) {
            if (error) {
                *error = QStringLiteral("Unexpected end of block data");
            }
            return false;
        }
        BlockedDriveEntry e = deserializeBlock(blob);
        if (!e.driveKey.isEmpty() || !e.uniqueId.isEmpty()) {
            out.blocks.append(e);
        }
    }

    return true;
}

QByteArray PolicyBlobCodec::sign(const QByteArray& payload, const QByteArray& key)
{
    unsigned int len = EVP_MAX_MD_SIZE;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int outLen = 0;
    if (!HMAC(EVP_sha256(), key.constData(), key.size(),
              reinterpret_cast<const unsigned char*>(payload.constData()),
              static_cast<size_t>(payload.size()), md, &outLen)) {
        return {};
    }
    return QByteArray(reinterpret_cast<char*>(md), static_cast<int>(outLen));
}

bool PolicyBlobCodec::verify(const QByteArray& payload, const QByteArray& signature, const QByteArray& key)
{
    const QByteArray expected = sign(payload, key);
    if (expected.size() != signature.size()) {
        return false;
    }
    return CRYPTO_memcmp(expected.constData(), signature.constData(),
                         static_cast<size_t>(signature.size())) == 0;
}

QByteArray PolicyBlobCodec::loadOrCreateKey()
{
    const QString path = PolicyPaths::keyFilePath();
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        QByteArray key = existing.readAll();
        existing.close();
        if (key.size() >= 32) {
            return key.left(32);
        }
    }

    QByteArray key(32, Qt::Uninitialized);
    for (int i = 0; i < 32; ++i) {
        key[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xFF);
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        out.write(key);
        out.close();
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    }
    return key;
}

} // namespace FlashSpartan::Policy
