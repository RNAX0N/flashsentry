#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace FlashSentry {

/** Resolved gpg(1) executable. Honors FLASHSENTRY_GPG_PROGRAM on every call. */
inline QString gpgProgram()
{
    const QByteArray env = qgetenv("FLASHSENTRY_GPG_PROGRAM");
    if (!env.isEmpty()) {
        const QString fromEnv = QString::fromUtf8(env);
        if (QFile::exists(fromEnv)) {
            return fromEnv;
        }
    }

    static QString cached;
    if (!cached.isEmpty()) {
        return cached;
    }

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
                gpg = candidate;
                break;
            }
        }
    }
#endif
    cached = gpg.isEmpty() ? QStringLiteral("gpg") : gpg;
    return cached;
}

inline QStringList gpgBatchArgs()
{
    QStringList args = {QStringLiteral("--batch"), QStringLiteral("--no-tty")};
#ifdef Q_OS_WIN
    args << QStringLiteral("--pinentry-mode") << QStringLiteral("loopback");
#endif
    return args;
}

/** Ensure gpg can find its bundled DLLs when spawned from MSVC/Qt on Windows. */
inline void configureGpgProcess(QProcess& proc)
{
    const QString gpg = gpgProgram();
    proc.setProgram(gpg);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("GNUPGHOME"));

#ifdef Q_OS_WIN
    const QFileInfo gpgInfo(gpg);
    if (gpgInfo.exists()) {
        const QString binDir = QDir::toNativeSeparators(gpgInfo.absolutePath());
        proc.setWorkingDirectory(binDir);
        const QString path = env.value(QStringLiteral("PATH"));
        if (!path.contains(binDir, Qt::CaseInsensitive)) {
            env.insert(QStringLiteral("PATH"), binDir + QLatin1Char(';') + path);
        }
    }
#endif

    proc.setProcessEnvironment(env);
}

} // namespace FlashSentry
