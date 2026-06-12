#include "IsoQuickFingerprint.h"

#include <openssl/evp.h>

#include <QFile>

namespace FlashSpartan {

namespace {

constexpr qint64 kChunkBytes = 1024 * 1024;

bool digestBytes(EVP_MD_CTX* ctx, const char* data, qint64 length)
{
    return EVP_DigestUpdate(ctx, data, static_cast<size_t>(length)) == 1;
}

bool digestSize(EVP_MD_CTX* ctx, qint64 sizeBytes)
{
    unsigned char sizeLe[8];
    for (int i = 0; i < 8; ++i) {
        sizeLe[i] = static_cast<unsigned char>((sizeBytes >> (8 * i)) & 0xFF);
    }
    return digestBytes(ctx, reinterpret_cast<const char*>(sizeLe), 8);
}

QString finalizeHex(EVP_MD_CTX* ctx)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &len) != 1) {
        return {};
    }
    return QByteArray(reinterpret_cast<char*>(hash), static_cast<int>(len)).toHex();
}

} // namespace

QString IsoQuickFingerprint::compute(const QString& filePath, QString* errorOut)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return {};
    }

    const qint64 sizeBytes = file.size();
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        if (errorOut) {
            *errorOut = QStringLiteral("OpenSSL context failed");
        }
        return {};
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1
        || !digestSize(ctx, sizeBytes)) {
        EVP_MD_CTX_free(ctx);
        if (errorOut) {
            *errorOut = QStringLiteral("OpenSSL hash initialization failed");
        }
        return {};
    }

    QByteArray buffer(kChunkBytes, Qt::Uninitialized);
    const qint64 firstRead = file.read(buffer.data(), kChunkBytes);
    if (firstRead < 0) {
        EVP_MD_CTX_free(ctx);
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return {};
    }
    if (firstRead > 0 && !digestBytes(ctx, buffer.constData(), firstRead)) {
        EVP_MD_CTX_free(ctx);
        if (errorOut) {
            *errorOut = QStringLiteral("OpenSSL hash update failed");
        }
        return {};
    }

    if (sizeBytes > kChunkBytes) {
        const qint64 tailOffset = qMax(kChunkBytes, sizeBytes - kChunkBytes);
        if (!file.seek(tailOffset)) {
            EVP_MD_CTX_free(ctx);
            if (errorOut) {
                *errorOut = file.errorString();
            }
            return {};
        }
        while (file.pos() < sizeBytes) {
            const qint64 toRead = qMin<qint64>(kChunkBytes, sizeBytes - file.pos());
            const qint64 n = file.read(buffer.data(), toRead);
            if (n < 0) {
                EVP_MD_CTX_free(ctx);
                if (errorOut) {
                    *errorOut = file.errorString();
                }
                return {};
            }
            if (n == 0) {
                break;
            }
            if (!digestBytes(ctx, buffer.constData(), n)) {
                EVP_MD_CTX_free(ctx);
                if (errorOut) {
                    *errorOut = QStringLiteral("OpenSSL hash update failed");
                }
                return {};
            }
        }
    }

    const QString hex = finalizeHex(ctx);
    EVP_MD_CTX_free(ctx);
    if (hex.isEmpty() && errorOut) {
        *errorOut = QStringLiteral("OpenSSL hash finalization failed");
    }
    return hex;
}

} // namespace FlashSpartan
