#include <QtTest>
#include <QTemporaryFile>

#include "RawDeviceHash.h"

#include <cerrno>
#include <unistd.h>

using namespace FlashSentry;

class TestRawDeviceHash : public QObject {
    Q_OBJECT

private slots:
    void normalizesBufferSizes();
    void rejectsNonDevPaths();
    void rejectsCharacterDevices();
};

void TestRawDeviceHash::normalizesBufferSizes()
{
    QCOMPARE(RawDeviceHash::normalizedBufferSizeKB(0), RawDeviceHash::kDefaultBufferSizeKB);
    QCOMPARE(RawDeviceHash::normalizedBufferSizeKB(-1), RawDeviceHash::kDefaultBufferSizeKB);
    QCOMPARE(RawDeviceHash::normalizedBufferSizeKB(1), RawDeviceHash::kMinBufferSizeKB);
    QCOMPARE(RawDeviceHash::normalizedBufferSizeKB(RawDeviceHash::kMaxBufferSizeKB + 1),
             RawDeviceHash::kMaxBufferSizeKB);
    QCOMPARE(RawDeviceHash::normalizedBufferSizeKB(4096), 4096);
}

void TestRawDeviceHash::rejectsNonDevPaths()
{
    QTemporaryFile file;
    QVERIFY(file.open());

    errno = 0;
    const int fd = RawDeviceHash::openDevice(file.fileName());
    if (fd >= 0) {
        close(fd);
    }
    QCOMPARE(fd, -1);
    QCOMPARE(errno, EINVAL);
}

void TestRawDeviceHash::rejectsCharacterDevices()
{
    errno = 0;
    const int fd = RawDeviceHash::openDevice(QStringLiteral("/dev/null"));
    if (fd >= 0) {
        close(fd);
    }
    QCOMPARE(fd, -1);
    QCOMPARE(errno, ENOTBLK);
}

QTEST_MAIN(TestRawDeviceHash)
#include "test_raw_device_hash.moc"
