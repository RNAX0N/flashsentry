#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>

#include "policy/PolicyPaths.h"
#include "policy/PolicySocketAuth.h"

using namespace FlashSpartan::Policy;

class TestPolicyDaemon : public QObject {
    Q_OBJECT

    QTemporaryDir m_configDir;
    QTemporaryDir m_runtimeDir;
    QString m_daemonPath;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void pingWithoutAuth();
    void snapshotRequiresAuthToken();
    void snapshotWithTokenSucceeds();

private:
    QJsonObject sendRequest(const QJsonObject& req) const;
    bool startDaemon();
    void stopDaemon();
    QProcess m_daemon;
};

static QString findPolicyDaemonBinary()
{
    const QString fromEnv = qEnvironmentVariable("FLASHSPARTAN_POLICYD_TEST_BIN");
    if (!fromEnv.isEmpty() && QFile::exists(fromEnv)) {
        return fromEnv;
    }
    const QDir buildDir(QCoreApplication::applicationDirPath());
    const QString sibling = buildDir.absoluteFilePath(QStringLiteral("flashspartan-policyd"));
    if (QFile::exists(sibling)) {
        return sibling;
    }
    return {};
}

void TestPolicyDaemon::initTestCase()
{
    QVERIFY(m_configDir.isValid());
    QVERIFY(m_runtimeDir.isValid());
    m_daemonPath = findPolicyDaemonBinary();
    if (m_daemonPath.isEmpty()) {
        QSKIP("flashspartan-policyd binary not found");
    }

    qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toUtf8());
    qputenv("FLASHSPARTAN_POLICY_CONFIG", m_configDir.path().toUtf8());
    QVERIFY2(startDaemon(), "failed to start flashspartan-policyd");
}

void TestPolicyDaemon::cleanupTestCase()
{
    stopDaemon();
}

bool TestPolicyDaemon::startDaemon()
{
    m_daemon.setProgram(m_daemonPath);
    m_daemon.setProcessChannelMode(QProcess::MergedChannels);
    m_daemon.start();
    if (!m_daemon.waitForStarted(3000)) {
        return false;
    }

    for (int i = 0; i < 50; ++i) {
        QJsonObject ping;
        ping.insert(QStringLiteral("op"), QStringLiteral("ping"));
        const QJsonObject resp = sendRequest(ping);
        if (resp.value(QStringLiteral("ok")).toBool()) {
            return QFile::exists(PolicyPaths::tokenPath());
        }
        QTest::qWait(50);
    }
    return false;
}

void TestPolicyDaemon::stopDaemon()
{
    if (m_daemon.state() != QProcess::NotRunning) {
        m_daemon.terminate();
        m_daemon.waitForFinished(2000);
    }
    QFile::remove(PolicyPaths::socketPath());
    QFile::remove(PolicyPaths::tokenPath());
}

QJsonObject TestPolicyDaemon::sendRequest(const QJsonObject& req) const
{
    QLocalSocket socket;
    socket.connectToServer(PolicyPaths::socketPath());
    if (!socket.waitForConnected(2000)) {
        return {};
    }
    socket.write(QJsonDocument(req).toJson(QJsonDocument::Compact));
    socket.write("\n");
    socket.flush();
    if (!socket.waitForReadyRead(5000)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(socket.readAll().trimmed());
    socket.disconnectFromServer();
    return doc.isObject() ? doc.object() : QJsonObject{};
}

void TestPolicyDaemon::pingWithoutAuth()
{
    QJsonObject req;
    req.insert(QStringLiteral("op"), QStringLiteral("ping"));
    const QJsonObject resp = sendRequest(req);
    QVERIFY(resp.value(QStringLiteral("ok")).toBool());
}

void TestPolicyDaemon::snapshotRequiresAuthToken()
{
    QJsonObject req;
    req.insert(QStringLiteral("op"), QStringLiteral("snapshot"));
    const QJsonObject resp = sendRequest(req);
    QVERIFY(!resp.value(QStringLiteral("ok")).toBool());
    QCOMPARE(resp.value(QStringLiteral("error")).toString(), QStringLiteral("unauthorized"));
}

void TestPolicyDaemon::snapshotWithTokenSucceeds()
{
    const QString token = PolicySocketAuth::readTokenFile(PolicyPaths::tokenPath());
    QVERIFY(!token.isEmpty());

    QJsonObject req;
    req.insert(QStringLiteral("op"), QStringLiteral("snapshot"));
    req.insert(QStringLiteral("auth"), token);
    const QJsonObject resp = sendRequest(req);
    QVERIFY(resp.value(QStringLiteral("ok")).toBool());
    QVERIFY(resp.contains(QStringLiteral("devices")));
}

QTEST_MAIN(TestPolicyDaemon)
#include "test_policy_daemon.moc"
