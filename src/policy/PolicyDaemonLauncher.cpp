#include "policy/PolicyDaemonLauncher.h"
#include "policy/PolicyDaemonClient.h"
#include "policy/PolicyPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#ifndef FLASHSENTRY_POLICYD_PATH
#define FLASHSENTRY_POLICYD_PATH ""
#endif

namespace FlashSentry::Policy {

QString PolicyDaemonLauncher::daemonExecutablePath()
{
    const QByteArray compiled = QByteArray(FLASHSENTRY_POLICYD_PATH);
    if (!compiled.isEmpty() && QFile::exists(QString::fromUtf8(compiled))) {
        return QString::fromUtf8(compiled);
    }

    const QString sibling =
        QCoreApplication::applicationDirPath() + QStringLiteral("/flashsentry-policyd");
    if (QFile::exists(sibling)) {
        return sibling;
    }

    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("flashsentry-policyd"));
    if (!inPath.isEmpty()) {
        return inPath;
    }

    return QString::fromUtf8(compiled);
}

bool PolicyDaemonLauncher::ensureRunning(QString* error)
{
    PolicyDaemonClient client;
    if (client.ping()) {
        return true;
    }

    const QString daemon = daemonExecutablePath();
    if (daemon.isEmpty() || !QFile::exists(daemon)) {
        if (error) {
            *error = QStringLiteral("flashsentry-policyd not found");
        }
        return false;
    }

    QProcess proc;
    proc.setProgram(daemon);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    if (!proc.startDetached()) {
        if (error) {
            *error = QStringLiteral("Failed to start flashsentry-policyd");
        }
        return false;
    }

    for (int i = 0; i < 30; ++i) {
        if (client.ping()) {
            return true;
        }
        QThread::msleep(100);
    }

    if (error) {
        *error = QStringLiteral("flashsentry-policyd did not become ready");
    }
    return false;
}

} // namespace FlashSentry::Policy
