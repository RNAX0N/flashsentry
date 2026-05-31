#pragma once

#include <QFile>
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
#ifdef Q_OS_WIN
    if (gpg.isEmpty()) {
        const QStringList bundled = {
            QStringLiteral("C:/Program Files/Git/usr/bin/gpg.exe"),
            QStringLiteral("C:/Program Files/Git/mingw64/bin/gpg.exe"),
            QStringLiteral("C:/msys64/usr/bin/gpg.exe"),
        };
        for (const QString& candidate : bundled) {
            if (QFile::exists(candidate)) {
                return candidate;
            }
        }
    }
#endif
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
