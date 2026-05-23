#include "MerkleTree.h"

#include <openssl/evp.h>

#include <algorithm>

namespace FlashSentry {

namespace {

QByteArray sha256(const QByteArray& data)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.constData(), static_cast<size_t>(data.size()));
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);
    return QByteArray(reinterpret_cast<const char*>(hash), static_cast<int>(len));
}

QByteArray sha256HexPair(const QByteArray& leftBin, const QByteArray& rightBin)
{
    const QByteArray payload = leftBin.toHex() + rightBin.toHex();
    return sha256(payload);
}

} // namespace

QByteArray MerkleTree::leafDigest(const QString& relativePath, const QString& contentHashHex)
{
    const QByteArray payload = relativePath.toUtf8() + '\0' + contentHashHex.toLatin1();
    return sha256(payload);
}

QByteArray MerkleTree::combine(const QByteArray& left, const QByteArray& right)
{
    return sha256HexPair(left, right);
}

QString MerkleTree::toHex(const QByteArray& data)
{
    return QString::fromLatin1(data.toHex());
}

MerkleTree MerkleTree::build(const QVector<Leaf>& leaves)
{
    QVector<Leaf> sorted = leaves;
    std::sort(sorted.begin(), sorted.end(), [](const Leaf& a, const Leaf& b) {
        return a.relativePath < b.relativePath;
    });

    for (Leaf& leaf : sorted) {
        leaf.digest = leafDigest(leaf.relativePath, leaf.contentHashHex);
    }

    if (sorted.isEmpty()) {
        return MerkleTree({}, QString());
    }

    QVector<QByteArray> level;
    level.reserve(sorted.size());
    for (const Leaf& leaf : sorted) {
        level.append(leaf.digest);
    }

    while (level.size() > 1) {
        QVector<QByteArray> next;
        next.reserve((level.size() + 1) / 2);
        for (int i = 0; i < level.size(); i += 2) {
            if (i + 1 < level.size()) {
                next.append(combine(level[i], level[i + 1]));
            } else {
                next.append(combine(level[i], level[i]));
            }
        }
        level = std::move(next);
    }

    return MerkleTree(std::move(sorted), toHex(level.first()));
}

QString MerkleTree::rootHex(const QVector<Leaf>& leaves)
{
    return build(leaves).rootHex();
}

MerkleTree::MerkleTree(QVector<Leaf> leaves, QString rootHex)
    : m_leaves(std::move(leaves))
    , m_rootHex(std::move(rootHex))
{
}

} // namespace FlashSentry
