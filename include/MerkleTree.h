#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>

namespace FlashSentry {

/**
 * @brief Binary Merkle tree over sorted leaf digests (hex SHA-256).
 */
class MerkleTree {
public:
    struct Leaf {
        QString relativePath;
        QString contentHashHex;
        QByteArray digest;
    };

    static QByteArray leafDigest(const QString& relativePath, const QString& contentHashHex);
    static QByteArray combine(const QByteArray& left, const QByteArray& right);
    static QString toHex(const QByteArray& data);

    static MerkleTree build(const QVector<Leaf>& leaves);
    static QString rootHex(const QVector<Leaf>& leaves);

    QString rootHex() const { return m_rootHex; }
    bool isEmpty() const { return m_leaves.isEmpty(); }
    int leafCount() const { return m_leaves.size(); }

private:
    explicit MerkleTree(QVector<Leaf> leaves, QString rootHex);

    QVector<Leaf> m_leaves;
    QString m_rootHex;
};

} // namespace FlashSentry
