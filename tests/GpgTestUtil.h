#pragma once

#include "GpgUtil.h"

#include <QProcess>
#include <QStandardPaths>
#include <QString>

namespace FlashSentryTest {

inline QString gpgProgram()
{
    return FlashSentry::gpgProgram();
}

inline bool gpgAvailable()
{
    const QString gpg = gpgProgram();
    if (gpg.isEmpty() || gpg == QStringLiteral("gpg")) {
        if (QStandardPaths::findExecutable(QStringLiteral("gpg")).isEmpty()
            && QStandardPaths::findExecutable(QStringLiteral("gpg.exe")).isEmpty()) {
            return false;
        }
    }
    QProcess proc;
    proc.setProgram(gpg);
    proc.setArguments(FlashSentry::gpgBatchArgs() << QStringLiteral("--version"));
    proc.start();
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        return false;
    }
    return proc.exitCode() == 0;
}

} // namespace FlashSentryTest
