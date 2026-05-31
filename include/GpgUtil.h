#pragma once

#include <QByteArray>
#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace FlashSentry {

/** Resolved gpg(1) executable (cached). Honors FLASHSENTRY_GPG_PROGRAM. */
inline QString gpgProgram()
{
    static QString cached;
    if (!cached.isEmpty()) {
        return cached;
    }

    const QByteArray env = qgetenv("FLASHSENTRY_GPG_PROGRAM");
    if (!env.isEmpty()) {
        const QString fromEnv = QString::fromUtf8(env);
        if (QFile::exists(fromEnv)) {
            cached = fromEnv;
            return cached;
        }
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
    return {QStringLiteral("--batch"), QStringLiteral("--no-tty")};
}

} // namespace FlashSentry
