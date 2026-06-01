#pragma once

#include "PolicySnapshot.h"

#include <QByteArray>

namespace FlashSpartan::Policy {

/** Signed custom on-disk format (not JSON). */
class PolicyBlobCodec {
public:
    static constexpr quint32 kMagic = 0x31505346; // 'FSP1' little-endian
    static constexpr quint16 kVersion = 1;

    static QByteArray encode(const PolicySnapshot& snapshot);
    static bool decode(const QByteArray& fileBytes, PolicySnapshot& out, QString* error = nullptr);

    static QByteArray sign(const QByteArray& payload, const QByteArray& key);
    static bool verify(const QByteArray& payload, const QByteArray& signature, const QByteArray& key);

    static QByteArray loadOrCreateKey();
};

} // namespace FlashSpartan::Policy
