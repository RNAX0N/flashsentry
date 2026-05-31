#pragma once

#include <QProcess>
#include <QStandardPaths>
#include <QString>

namespace FlashSentryTest {

inline QString gpgProgram()
{
    QString gpg = QStandardPaths::findExecutable(QStringLiteral("gpg"));
    if (gpg.isEmpty()) {
        gpg = QStandardPaths::findExecutable(QStringLiteral("gpg.exe"));
    }
    return gpg;
}

inline bool gpgAvailable()
{
    const QString gpg = gpgProgram();
    if (gpg.isEmpty()) {
        return false;
    }
    QProcess proc;
    proc.setProgram(gpg);
    proc.setArguments({QStringLiteral("--version")});
    proc.start();
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        return false;
    }
    return proc.exitCode() == 0;
}

} // namespace FlashSentryTest
