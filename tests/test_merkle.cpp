#include <QtTest>
#include "MerkleTree.h"

using namespace FlashSentry;

class TestMerkle : public QObject {
    Q_OBJECT
private slots:
    void deterministicRoot();
    void orderMatters();
};

void TestMerkle::deterministicRoot()
{
    QVector<MerkleTree::Leaf> leaves;
    MerkleTree::Leaf a;
    a.relativePath = QStringLiteral("a.txt");
    a.contentHashHex = QStringLiteral("aa");
    MerkleTree::Leaf b;
    b.relativePath = QStringLiteral("b.txt");
    b.contentHashHex = QStringLiteral("bb");
    leaves << a << b;

    const QString r1 = MerkleTree::rootHex(leaves);
    const QString r2 = MerkleTree::rootHex(leaves);
    QCOMPARE(r1, r2);
    QVERIFY(!r1.isEmpty());
}

void TestMerkle::orderMatters()
{
    MerkleTree::Leaf a;
    a.relativePath = QStringLiteral("a.txt");
    a.contentHashHex = QStringLiteral("aa");
    MerkleTree::Leaf b;
    b.relativePath = QStringLiteral("b.txt");
    b.contentHashHex = QStringLiteral("bb");

    const QString forward = MerkleTree::rootHex({a, b});
    const QString reverse = MerkleTree::rootHex({b, a});
    QVERIFY(forward != reverse);
}

QTEST_MAIN(TestMerkle)
#include "test_merkle.moc"
