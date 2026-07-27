#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

#include "ThemeMetadata.h"

class Config;
class Settings;

class ThemeManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY themesChanged)
    Q_PROPERTY(QString currentThemeId READ currentThemeId NOTIFY
                   currentThemeIdChanged)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY colorsChanged)
    Q_PROPERTY(QVariantMap fonts READ fonts NOTIFY fontsChanged)
    Q_PROPERTY(QVariantMap wallpaper READ wallpaper NOTIFY wallpaperChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        VersionRole,
        AuthorRole,
        DescriptionRole,
        PreviewPathRole,
        BuiltInRole
    };

    explicit ThemeManager(Config *config, Settings *settings,
                          QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentThemeId() const;
    QVariantMap colors() const;
    QVariantMap fonts() const;
    QVariantMap wallpaper() const;
    bool dark() const { return m_dark; }
    void setDark(bool dark);
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool installTheme(const QString &archivePath);
    Q_INVOKABLE bool switchTheme(const QString &themeId);
    Q_INVOKABLE bool uninstallTheme(const QString &themeId);

  signals:
    void themesChanged();
    void currentThemeIdChanged();
    void colorsChanged();
    void fontsChanged();
    void wallpaperChanged();
    void darkChanged();
    void lastErrorChanged();
    void themeInstalled(const QString &themeId);
    void themeInstallFailed(const QString &error);

  private:
    void setError(const QString &error);
    int indexOf(const QString &themeId) const;
    const ThemeMetadata &currentTheme() const;

    Config *m_config = nullptr;
    Settings *m_settings = nullptr;
    QVector<ThemeMetadata> m_themes;
    bool m_dark = false;
    QString m_lastError;
};
