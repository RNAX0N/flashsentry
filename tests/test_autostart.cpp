#include <QtTest>
#include <QFile>
#include <QStandardPaths>

#include "AutostartManager.h"

using namespace FlashSentry;

class TestAutostart : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void xdgAutostartEnableDisable();

private:
    QString m_autostartPath;
};

void TestAutostart::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(AutostartManager::isAvailable());
    if (AutostartManager::backend() != AutostartManager::Backend::Xdg) {
        QSKIP("XDG autostart backend required (no packaged systemd unit in this environment)");
    }
    m_autostartPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart/flashsentry-autostart.desktop");
}

void TestAutostart::cleanup()
{
    AutostartManager::setLoginAutostartEnabled(false, nullptr);
}

void TestAutostart::xdgAutostartEnableDisable()
{
    QString error;
    QVERIFY(AutostartManager::setLoginAutostartEnabled(true, &error));
    const auto enabled = AutostartManager::isLoginAutostartEnabled();
    QVERIFY(enabled.has_value());
    QVERIFY(*enabled);

    QVERIFY(QFile::exists(m_autostartPath));
    QFile file(m_autostartPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("Exec=")));
    QVERIFY(contents.contains(QStringLiteral("--minimized")));

    QVERIFY(AutostartManager::setLoginAutostartEnabled(false, &error));
    const auto disabled = AutostartManager::isLoginAutostartEnabled();
    QVERIFY(disabled.has_value());
    QVERIFY(!*disabled);
    QVERIFY(!QFile::exists(m_autostartPath));
}

QTEST_MAIN(TestAutostart)
#include "test_autostart.moc"
