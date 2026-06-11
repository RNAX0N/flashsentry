#include <QtTest>
#include <QTemporaryDir>
#include <QJsonObject>

#include "policy/PolicySocketAuth.h"

using namespace FlashSpartan::Policy;

class TestPolicyAuth : public QObject {
    Q_OBJECT

private slots:
    void tokenRoundTrip();
    void requestRequiresMatchingToken();
    void pingStyleOpsStillNeedAuthWhenChecked();
};

void TestPolicyAuth::tokenRoundTrip()
{
    const QString token = PolicySocketAuth::generateToken();
    QVERIFY(token.size() >= 64);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("policy.token"));
    QString err;
    QVERIFY(PolicySocketAuth::writeTokenFile(path, token, &err));
    QCOMPARE(PolicySocketAuth::readTokenFile(path), token);
}

void TestPolicyAuth::requestRequiresMatchingToken()
{
    const QString token = QStringLiteral("abc123");
    QJsonObject req;
    req.insert(QStringLiteral("op"), QStringLiteral("snapshot"));
    req.insert(QStringLiteral("auth"), token);
    QVERIFY(PolicySocketAuth::requestAuthorized(req, token));
    QVERIFY(!PolicySocketAuth::requestAuthorized(req, QStringLiteral("wrong")));
    QVERIFY(!PolicySocketAuth::requestAuthorized(QJsonObject{}, token));
}

void TestPolicyAuth::pingStyleOpsStillNeedAuthWhenChecked()
{
    QJsonObject req;
    req.insert(QStringLiteral("op"), QStringLiteral("ping"));
    QVERIFY(!PolicySocketAuth::requestAuthorized(req, QStringLiteral("secret")));
}

QTEST_MAIN(TestPolicyAuth)
#include "test_policy_auth.moc"
