#include "AutoStartManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

#include "Settings.h"

namespace
{
QString quotedExecutablePath()
{
    return QLatin1Char('"') +
           QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) +
           QLatin1Char('"');
}

QString linuxDesktopFilePath()
{
    const QString configHome =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configHome.isEmpty())
        return QString();
    return QDir(configHome)
        .filePath(QStringLiteral("autostart/StarryAgent.desktop"));
}
} // namespace

AutoStartManager::AutoStartManager(Settings *settings, QObject *parent)
    : QObject(parent)
{
    if (!settings)
        return;
    connect(settings, &Settings::startOnLoginChanged, this,
            [this, settings] { apply(settings->startOnLogin()); });
    apply(settings->startOnLogin());
}

bool AutoStartManager::supported() const
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

QString AutoStartManager::appDisplayName()
{
    return QStringLiteral("StarryAgent");
}

void AutoStartManager::apply(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings runKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    if (enabled)
        runKey.setValue(appDisplayName(), quotedExecutablePath());
    else
        runKey.remove(appDisplayName());
#elif defined(Q_OS_LINUX)
    const QString desktopPath = linuxDesktopFilePath();
    if (desktopPath.isEmpty())
        return;
    if (!enabled)
    {
        QFile::remove(desktopPath);
        return;
    }

    const QFileInfo info(desktopPath);
    QDir().mkpath(info.absolutePath());
    QFile file(desktopPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                   QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream << "[Desktop Entry]\n";
    stream << "Type=Application\n";
    stream << "Version=1.0\n";
    stream << "Name=" << appDisplayName() << "\n";
    stream << "Exec=" << quotedExecutablePath() << "\n";
    stream << "Terminal=false\n";
    stream << "X-GNOME-Autostart-enabled=true\n";
    file.close();
#else
    Q_UNUSED(enabled);
#endif
}
