#include "AutostartManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace FlashSentry {

namespace {

#ifdef Q_OS_WIN

constexpr auto kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr auto kRunValue = "FlashSentry";

QString flashsentryExecutable()
{
    const QString app = QCoreApplication::applicationFilePath();
    return app.isEmpty() ? QStringLiteral("flashsentry.exe") : app;
}

QString quotedAutostartCommand()
{
    QString exe = flashsentryExecutable();
    exe.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\" --minimized").arg(exe);
}

#else

constexpr auto kServiceName = "flashsentry.service";
constexpr auto kAutostartFileName = "flashsentry-autostart.desktop";

bool commandExists(const char* name)
{
    QProcess which;
    which.start(QStringLiteral("which"), {QString::fromLatin1(name)});
    if (!which.waitForFinished(3000)) {
        return false;
    }
    return which.exitCode() == 0;
}

bool runSystemctl(const QStringList& args, QString* stdOut, QString* stdErr, int timeoutMs = 10000)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("systemctl"));
    proc.setArguments(QStringList{QStringLiteral("--user")} + args);
    proc.start();
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        if (stdErr) {
            *stdErr = QStringLiteral("systemctl timed out");
        }
        return false;
    }
    if (stdOut) {
        *stdOut = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }
    if (stdErr) {
        *stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    }
    return proc.exitCode() == 0;
}

bool systemdUnitExists()
{
    QString out;
    QString err;
    runSystemctl({QStringLiteral("cat"), QString::fromLatin1(kServiceName)}, &out, &err, 3000);
    return !out.isEmpty();
}

QString xdgAutostartPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart");
    return dir + QStringLiteral("/") + QString::fromLatin1(kAutostartFileName);
}

QString flashsentryExecutable()
{
    const QString app = QCoreApplication::applicationFilePath();
    if (!app.isEmpty()) {
        return app;
    }
    return QStringLiteral("flashsentry");
}

bool writeXdgAutostart(QString* errorMessage)
{
    const QString path = xdgAutostartPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not write %1").arg(path);
        }
        return false;
    }

    QTextStream out(&file);
    out << "[Desktop Entry]\n"
        << "Type=Application\n"
        << "Name=FlashSentry\n"
        << "Comment=USB and ISO security monitor\n"
        << "Exec=" << flashsentryExecutable() << " --minimized\n"
        << "Icon=flashsentry\n"
        << "Terminal=false\n"
        << "X-GNOME-Autostart-enabled=true\n";

    return true;
}

std::optional<bool> systemdAutostartEnabled()
{
    QString out;
    QString err;
    if (runSystemctl({QStringLiteral("is-enabled"), QString::fromLatin1(kServiceName)}, &out, &err, 5000)) {
        return true;
    }
    const QString combined = out + QLatin1Char(' ') + err;
    if (combined.contains(QLatin1String("disabled"), Qt::CaseInsensitive)) {
        return false;
    }
    if (combined.contains(QLatin1String("not-found"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }
    return std::nullopt;
}

#endif

} // namespace

bool AutostartManager::isAvailable()
{
    return backend() != Backend::None;
}

AutostartManager::Backend AutostartManager::backend()
{
#ifdef Q_OS_WIN
    return Backend::WindowsRegistry;
#else
    if (commandExists("systemctl") && systemdUnitExists()) {
        return Backend::Systemd;
    }
    const QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/autostart");
    if (!autostartDir.isEmpty()) {
        return Backend::Xdg;
    }
    return Backend::None;
#endif
}

std::optional<bool> AutostartManager::isLoginAutostartEnabled()
{
#ifdef Q_OS_WIN
    QSettings runKey(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    const QString value = runKey.value(QString::fromLatin1(kRunValue)).toString();
    if (value.isEmpty()) {
        return false;
    }
    return value.contains(flashsentryExecutable(), Qt::CaseInsensitive);
#else
    if (commandExists("systemctl") && systemdUnitExists()) {
        const auto systemdState = systemdAutostartEnabled();
        if (systemdState.has_value()) {
            return systemdState;
        }
    }
    if (backend() == Backend::None && !QDir().exists(QFileInfo(xdgAutostartPath()).absolutePath())) {
        return std::nullopt;
    }
    return QFile::exists(xdgAutostartPath());
#endif
}

bool AutostartManager::setLoginAutostartEnabled(bool enabled, QString* errorMessage)
{
#ifdef Q_OS_WIN
    QSettings runKey(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        runKey.setValue(QString::fromLatin1(kRunValue), quotedAutostartCommand());
    } else {
        runKey.remove(QString::fromLatin1(kRunValue));
    }
    runKey.sync();
    if (runKey.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not update Windows Run registry key");
        }
        return false;
    }
    return true;
#else
    if (commandExists("systemctl") && systemdUnitExists()) {
        QString err;
        const QStringList args = enabled
            ? QStringList{QStringLiteral("enable"), QStringLiteral("--now"),
                          QString::fromLatin1(kServiceName)}
            : QStringList{QStringLiteral("disable"), QString::fromLatin1(kServiceName)};
        if (runSystemctl(args, nullptr, &err)) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = err.isEmpty()
                ? QStringLiteral("systemctl --user failed")
                : err;
        }
        return false;
    }

    if (backend() == Backend::Xdg || QDir().mkpath(QFileInfo(xdgAutostartPath()).absolutePath())) {
        if (enabled) {
            return writeXdgAutostart(errorMessage);
        }
        if (QFile::exists(xdgAutostartPath()) && !QFile::remove(xdgAutostartPath())) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not remove autostart entry");
            }
            return false;
        }
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral(
            "Install flashsentry.service (Arch package) or run from a desktop session with XDG config.");
    }
    return false;
#endif
}

QString AutostartManager::backendDescription()
{
    switch (backend()) {
    case Backend::Systemd:
        return QStringLiteral("systemd user service (flashsentry.service)");
    case Backend::Xdg:
        return QStringLiteral("desktop autostart entry");
    case Backend::WindowsRegistry:
        return QStringLiteral("Windows Run registry key");
    case Backend::None:
        return QString();
    }
    return QString();
}

} // namespace FlashSentry
