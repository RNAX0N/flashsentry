/**
 * Polkit-privileged helper for raw USB block device reads.
 * Invoked via: pkexec /usr/lib/flashsentry/flashsentry-read-helper hash <dev> <algo> <buffer_kb> <mmap>
 * Prints one JSON line to stdout.
 */

#include "RawDeviceHash.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QElapsedTimer>

#include <cerrno>
#include <cstring>

using namespace FlashSentry;

static void printResult(const HashResult& result, qint64 durationMs)
{
    QJsonObject obj;
    if (result.success) {
        obj[QStringLiteral("success")] = true;
        obj[QStringLiteral("hash")] = result.hash;
        obj[QStringLiteral("algorithm")] = result.algorithm;
        obj[QStringLiteral("bytes")] = static_cast<double>(result.bytesProcessed);
        obj[QStringLiteral("duration_ms")] = durationMs;
    } else {
        obj[QStringLiteral("success")] = false;
        obj[QStringLiteral("error")] = result.errorMessage;
    }

    QTextStream out(stdout);
    out << QJsonDocument(obj).toJson(QJsonDocument::Compact) << "\n";
    out.flush();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("flashsentry-read-helper"));

    const QStringList args = app.arguments();
    if (args.size() < 7 || args[1] != QStringLiteral("hash")) {
        QTextStream err(stderr);
        err << "Usage: flashsentry-read-helper hash <device> <algorithm> <buffer_kb> <use_mmap 0|1>\n";
        return 2;
    }

    RawDeviceHash::Options options;
    options.deviceNode = args[2];
    options.algorithm = RawDeviceHash::algorithmFromName(args[3]);
    options.bufferSizeKB = args[4].toInt();
    options.useMemoryMapping = args[5] != QStringLiteral("0");

    QElapsedTimer timer;
    timer.start();

    const int fd = RawDeviceHash::openDevice(options.deviceNode);
    if (fd < 0) {
        HashResult fail;
        fail.deviceNode = options.deviceNode;
        fail.errorMessage = QString("Failed to open device: %1").arg(strerror(errno));
        printResult(fail, timer.elapsed());
        return 1;
    }

    HashResult result = RawDeviceHash::hashOpenFd(fd, options);
    RawDeviceHash::closeDevice(fd);
    result.durationMs = static_cast<uint64_t>(timer.elapsed());

    printResult(result, timer.elapsed());
    return result.success ? 0 : 1;
}
