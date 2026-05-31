/**
 * Polkit-privileged helper for raw USB block device reads.
 * Invoked via: pkexec /usr/lib/flashsentry/flashsentry-read-helper hash <dev> <algo> <buffer_kb> <mmap>
 * Prints one JSON line to stdout.
 */

#include "RawDeviceHash.h"

#include <QCoreApplication>
#include <QFile>
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
    QString outputPath;
    QStringList positional;
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QStringLiteral("--output") && i + 1 < args.size()) {
            outputPath = args.at(i + 1);
            ++i;
            continue;
        }
        positional.append(args.at(i));
    }

    if (positional.size() != 5 || positional.at(0) != QStringLiteral("hash")) {
        QTextStream err(stderr);
        err << "Usage: flashsentry-read-helper hash <device> <algorithm> <buffer_kb> "
               "<use_mmap 0|1> [--output path]\n";
        return 2;
    }

    QElapsedTimer timer;
    timer.start();

    bool bufferSizeOk = false;
    const int requestedBufferSizeKB = positional.at(3).toInt(&bufferSizeOk);
    if (!bufferSizeOk) {
        HashResult fail;
        fail.deviceNode = positional.at(1);
        fail.errorMessage = QStringLiteral("Invalid buffer size");
        printResult(fail, timer.elapsed());
        return 2;
    }
    if (positional.at(4) != QStringLiteral("0") && positional.at(4) != QStringLiteral("1")) {
        HashResult fail;
        fail.deviceNode = positional.at(1);
        fail.errorMessage = QStringLiteral("Invalid memory-mapping option");
        printResult(fail, timer.elapsed());
        return 2;
    }

    RawDeviceHash::Options options;
    options.deviceNode = positional.at(1);
    options.algorithm = RawDeviceHash::algorithmFromName(positional.at(2));
    options.bufferSizeKB = RawDeviceHash::normalizedBufferSizeKB(requestedBufferSizeKB);
    options.useMemoryMapping = positional.at(4) != QStringLiteral("0");

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

    if (!outputPath.isEmpty()) {
        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream err(stderr);
            err << "Failed to write result file\n";
            return 1;
        }
        QJsonObject obj;
        if (result.success) {
            obj[QStringLiteral("success")] = true;
            obj[QStringLiteral("hash")] = result.hash;
            obj[QStringLiteral("algorithm")] = result.algorithm;
            obj[QStringLiteral("bytes")] = static_cast<double>(result.bytesProcessed);
            obj[QStringLiteral("duration_ms")] = static_cast<double>(timer.elapsed());
        } else {
            obj[QStringLiteral("success")] = false;
            obj[QStringLiteral("error")] = result.errorMessage;
        }
        outFile.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        outFile.close();
    } else {
        printResult(result, timer.elapsed());
    }
    return result.success ? 0 : 1;
}
