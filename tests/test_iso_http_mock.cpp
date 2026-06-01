#include <QtTest>
#include <QFile>

#include "IsoHttpClient.h"

using namespace FlashSpartan;

class TestIsoHttpMock : public QObject {
    Q_OBJECT

private slots:
    void mockHandlerReturnsFixtureBytes();
};

void TestIsoHttpMock::mockHandlerReturnsFixtureBytes()
{
    const QString payload = QStringLiteral("mock-http-payload");
    IsoHttpClient::setHandler([&payload](const QString& url, QString* errorOut, int) -> QByteArray {
        if (url == QStringLiteral("https://mock.flashspartan.test/data.txt")) {
            return payload.toUtf8();
        }
        if (errorOut) {
            *errorOut = QStringLiteral("unknown url");
        }
        return {};
    });

    QString err;
    const QByteArray data =
        IsoHttpClient::get(QStringLiteral("https://mock.flashspartan.test/data.txt"), &err);
    QCOMPARE(data, payload.toUtf8());
    QVERIFY(err.isEmpty());

    IsoHttpClient::reset();
}

QTEST_MAIN(TestIsoHttpMock)
#include "test_iso_http_mock.moc"
