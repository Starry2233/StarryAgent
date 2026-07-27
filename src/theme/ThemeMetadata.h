#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

class ThemeMetadata
{
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString version MEMBER version)
    Q_PROPERTY(QString author MEMBER author)
    Q_PROPERTY(QString description MEMBER description)

  public:
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    QVariantMap colorsLight;
    QVariantMap colorsDark;
    QVariantMap fonts;
    QVariantMap wallpaperLight;
    QVariantMap wallpaperDark;
    QString previewPath;
    QString themePath;

    bool valid() const { return !id.isEmpty() && !name.isEmpty(); }
    QVariantMap toVariantMap() const;

    static ThemeMetadata builtIn();
    static ThemeMetadata fromJson(const QJsonObject &json,
                                  const QString &themePath,
                                  QString *error = nullptr);
};

Q_DECLARE_METATYPE(ThemeMetadata)
