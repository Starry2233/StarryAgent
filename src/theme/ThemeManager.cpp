#include "ThemeManager.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include "ThemeLoader.h"
#include "core/Config.h"
#include "core/Settings.h"

ThemeManager::ThemeManager(Config *config, Settings *settings, QObject *parent)
    : QAbstractListModel(parent), m_config(config), m_settings(settings)
{
    reload();
    if (m_config)
        connect(m_config, &Config::rootDirChanged, this, &ThemeManager::reload);
    if (m_settings)
    {
        connect(m_settings, &Settings::currentThemeIdChanged, this,
                [this]
                {
                    emit currentThemeIdChanged();
                    emit colorsChanged();
                    emit fontsChanged();
                    emit wallpaperChanged();
                });
        connect(m_settings, &Settings::themeChanged, this,
                [this]
                {
                    setDark(m_settings &&
                            m_settings->theme() == QStringLiteral("dark"));
                });
        setDark(m_settings->theme() == QStringLiteral("dark"));
    }
}

int ThemeManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_themes.size();
}

QVariant ThemeManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_themes.size())
        return {};
    const ThemeMetadata &theme = m_themes.at(index.row());
    switch (role)
    {
    case IdRole:
        return theme.id;
    case NameRole:
        return theme.name;
    case VersionRole:
        return theme.version;
    case AuthorRole:
        return theme.author;
    case DescriptionRole:
        return theme.description;
    case PreviewPathRole:
        return theme.previewPath;
    case BuiltInRole:
        return theme.id == QStringLiteral("warm-clay");
    default:
        return {};
    }
}

QHash<int, QByteArray> ThemeManager::roleNames() const
{
    return {{IdRole, "themeId"},
            {NameRole, "name"},
            {VersionRole, "version"},
            {AuthorRole, "author"},
            {DescriptionRole, "description"},
            {PreviewPathRole, "previewPath"},
            {BuiltInRole, "builtIn"}};
}

QString ThemeManager::currentThemeId() const
{
    return m_settings ? m_settings->currentThemeId() : QStringLiteral("warm-clay");
}

QVariantMap ThemeManager::colors() const
{
    const ThemeMetadata &theme = currentTheme();
    return m_dark ? theme.colorsDark : theme.colorsLight;
}

QVariantMap ThemeManager::fonts() const { return currentTheme().fonts; }

QVariantMap ThemeManager::wallpaper() const
{
    const ThemeMetadata &theme = currentTheme();
    return m_dark ? theme.wallpaperDark : theme.wallpaperLight;
}

void ThemeManager::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    emit darkChanged();
    emit colorsChanged();
    emit wallpaperChanged();
}

void ThemeManager::reload()
{
    beginResetModel();
    m_themes.clear();
    m_themes.append(ThemeMetadata::builtIn());

    if (m_config && !m_config->themesPath().isEmpty())
    {
        QDir root(m_config->themesPath());
        root.mkpath(QStringLiteral("."));
        const QFileInfoList dirs =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &dir : dirs)
        {
            QString error;
            ThemeMetadata theme =
                ThemeLoader::loadTheme(dir.absoluteFilePath(), &error);
            if (!theme.valid() || indexOf(theme.id) >= 0)
                continue;
            ThemeLoader::loadThemeFonts(theme);
            m_themes.append(theme);
        }
    }
    endResetModel();
    emit themesChanged();
    emit currentThemeIdChanged();
    emit colorsChanged();
    emit fontsChanged();
    emit wallpaperChanged();
}

bool ThemeManager::installTheme(const QString &archivePath)
{
#ifdef Q_OS_ANDROID
    if (!m_settings || !m_settings->developerSettingsEnabled() ||
        !m_settings->developerThemeOnAndroidEnabled())
    {
        Q_UNUSED(archivePath);
        setError(QStringLiteral("Theme packages are not supported on Android."));
        emit themeInstallFailed(m_lastError);
        return false;
    }
#endif
    if (!m_config || m_config->themesPath().isEmpty())
    {
        setError(QStringLiteral("Theme storage is not ready."));
        emit themeInstallFailed(m_lastError);
        return false;
    }

    const QString tempId =
        QStringLiteral(".install-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString tempPath = QDir(m_config->themesPath()).filePath(tempId);
    QString error;
    if (!ThemeLoader::extractTheme(archivePath, tempPath, &error))
    {
        setError(error);
        emit themeInstallFailed(error);
        return false;
    }

    ThemeMetadata theme = ThemeLoader::loadTheme(tempPath, &error);
    if (!theme.valid())
    {
        QDir(tempPath).removeRecursively();
        setError(error);
        emit themeInstallFailed(error);
        return false;
    }

    const QString finalPath = QDir(m_config->themesPath()).filePath(theme.id);
    if (theme.id == QStringLiteral("warm-clay"))
    {
        QDir(tempPath).removeRecursively();
        setError(QStringLiteral("Cannot replace the built-in theme."));
        emit themeInstallFailed(m_lastError);
        return false;
    }
    if (QFileInfo::exists(finalPath))
        QDir(finalPath).removeRecursively();
    if (!QDir().rename(tempPath, finalPath))
    {
        QDir(tempPath).removeRecursively();
        setError(QStringLiteral("Failed to move theme into storage."));
        emit themeInstallFailed(m_lastError);
        return false;
    }

    reload();
    ThemeLoader::loadThemeFonts(currentTheme());
    emit themeInstalled(theme.id);
    return switchTheme(theme.id);
}

bool ThemeManager::switchTheme(const QString &themeId)
{
    const QString id = themeId.trimmed();
    if (indexOf(id) < 0)
    {
        setError(QStringLiteral("Theme is not installed: %1").arg(id));
        return false;
    }
    if (m_settings)
        m_settings->setCurrentThemeId(id);
    emit currentThemeIdChanged();
    emit colorsChanged();
    emit fontsChanged();
    emit wallpaperChanged();
    return true;
}

bool ThemeManager::uninstallTheme(const QString &themeId)
{
    const QString id = themeId.trimmed();
    if (id == QStringLiteral("warm-clay"))
    {
        setError(QStringLiteral("The built-in theme cannot be uninstalled."));
        return false;
    }
    const int idx = indexOf(id);
    if (idx < 0)
    {
        setError(QStringLiteral("Theme is not installed: %1").arg(id));
        return false;
    }
    if (currentThemeId() == id)
        switchTheme(QStringLiteral("warm-clay"));
    const QString path = m_themes.at(idx).themePath;
    QDir(path).removeRecursively();
    reload();
    return true;
}

void ThemeManager::setError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

int ThemeManager::indexOf(const QString &themeId) const
{
    for (int i = 0; i < m_themes.size(); ++i)
    {
        if (m_themes.at(i).id == themeId)
            return i;
    }
    return -1;
}

const ThemeMetadata &ThemeManager::currentTheme() const
{
    const int idx = indexOf(currentThemeId());
    if (idx >= 0)
        return m_themes.at(idx);
    return m_themes.first();
}
