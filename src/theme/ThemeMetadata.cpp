#include "ThemeMetadata.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonValue>
#include <QUrl>

namespace
{
QVariantMap jsonObjectToVariantMap(const QJsonObject &object)
{
    QVariantMap out;
    for (auto it = object.begin(); it != object.end(); ++it)
        out.insert(it.key(), it.value().toVariant());
    return out;
}

QString safeThemeFileUrl(const QString &themePath, const QString &relativePath,
                         const QStringList &allowedSuffixes,
                         QString *error = nullptr)
{
    const QString trimmed = relativePath.trimmed();
    if (trimmed.isEmpty())
        return QString();
    if (QDir::isAbsolutePath(trimmed) || trimmed.contains(QStringLiteral("..")))
    {
        if (error)
            *error = QStringLiteral("Theme file path is not allowed: %1")
                         .arg(relativePath);
        return QString();
    }

    const QFileInfo info(QDir(themePath).filePath(trimmed));
    const QString suffix = info.suffix().toLower();
    if (!allowedSuffixes.contains(suffix))
    {
        if (error)
            *error = QStringLiteral("Unsupported theme file format: %1")
                         .arg(relativePath);
        return QString();
    }
    if (!info.exists() || !info.isFile())
    {
        if (error)
            *error = QStringLiteral("Theme file not found: %1").arg(relativePath);
        return QString();
    }
    return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
}

bool validateThemeImage(const QString &fileUrl, QString *error)
{
    if (fileUrl.isEmpty())
        return true;
    const QString path = QUrl(fileUrl).toLocalFile();
    QImageReader reader(path);
    const QSize size = reader.size();
    if (!size.isValid())
    {
        if (error)
            *error = QStringLiteral("Theme image cannot be decoded: %1")
                         .arg(path);
        return false;
    }
    if (size.width() > 8192 || size.height() > 8192)
    {
        if (error)
            *error = QStringLiteral("Theme image is too large: %1x%2")
                         .arg(size.width())
                         .arg(size.height());
        return false;
    }
    const qint64 decodedBytes =
        qint64(size.width()) * qint64(size.height()) * 4;
    if (decodedBytes > 80ll * 1024 * 1024)
    {
        if (error)
            *error = QStringLiteral("Theme image decoded size exceeds 80MB.");
        return false;
    }
    return true;
}

QVariantMap parseFontMap(const QJsonObject &fonts, const QString &themePath)
{
    QVariantMap out;
    const QStringList allowed = {QStringLiteral("ttf"), QStringLiteral("otf")};
    for (const QString &key : {QStringLiteral("display"), QStringLiteral("body"),
                               QStringLiteral("mono")})
    {
        const QJsonObject font = fonts.value(key).toObject();
        const QString family = font.value(QStringLiteral("family")).toString();
        const QString file = font.value(QStringLiteral("file")).toString();
        QVariantMap entry;
        if (!family.trimmed().isEmpty())
            entry.insert(QStringLiteral("family"), family.trimmed());
        QString error;
        const QString url = safeThemeFileUrl(themePath, file, allowed, &error);
        if (!url.isEmpty())
            entry.insert(QStringLiteral("source"), url);
        if (!entry.isEmpty())
            out.insert(key, entry);
    }
    return out;
}

QVariantMap parseWallpaper(const QJsonObject &wallpaper,
                           const QString &themePath,
                           QString *error)
{
    QVariantMap out;
    const QString file = wallpaper.value(QStringLiteral("file")).toString();
    const QString url = safeThemeFileUrl(
        themePath, file,
        {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
         QStringLiteral("webp")},
        error);
    if (!url.isEmpty())
    {
        if (!validateThemeImage(url, error))
            return {};
        out.insert(QStringLiteral("source"), url);
    }

    const QString mode =
        wallpaper.value(QStringLiteral("mode")).toString(QStringLiteral("cover"))
            .trimmed()
            .toLower();
    out.insert(QStringLiteral("mode"),
               (mode == QStringLiteral("contain") ||
                mode == QStringLiteral("tile"))
                   ? mode
                   : QStringLiteral("cover"));

    const double opacity =
        wallpaper.value(QStringLiteral("opacity")).toDouble(0.0);
    out.insert(QStringLiteral("opacity"), qBound(0.0, opacity, 1.0));
    return out;
}
} // namespace

QVariantMap ThemeMetadata::toVariantMap() const
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("version"), version},
            {QStringLiteral("author"), author},
            {QStringLiteral("description"), description},
            {QStringLiteral("previewPath"), previewPath},
            {QStringLiteral("themePath"), themePath}};
}

ThemeMetadata ThemeMetadata::builtIn()
{
    ThemeMetadata theme;
    theme.id = QStringLiteral("warm-clay");
    theme.name = QStringLiteral("Warm Clay");
    theme.version = QStringLiteral("1.0.0");
    theme.author = QStringLiteral("StarryAgent");
    theme.description = QStringLiteral("Built-in warm paper and clay theme.");
    theme.colorsLight = {
        {QStringLiteral("paper"), QStringLiteral("#F5EFE3")},
        {QStringLiteral("surface"), QStringLiteral("#FBF6EC")},
        {QStringLiteral("surfaceAlt"), QStringLiteral("#EFE7D6")},
        {QStringLiteral("ink"), QStringLiteral("#1C1916")},
        {QStringLiteral("inkSoft"), QStringLiteral("#6B6357")},
        {QStringLiteral("line"), QStringLiteral("#D9CFBC")},
        {QStringLiteral("clay"), QStringLiteral("#C2502A")},
        {QStringLiteral("clayDeep"), QStringLiteral("#9E3D1F")},
        {QStringLiteral("moss"), QStringLiteral("#4A5340")},
        {QStringLiteral("shadowColor"), QStringLiteral("#1C140F")}};
    theme.colorsDark = {
        {QStringLiteral("paper"), QStringLiteral("#1A1714")},
        {QStringLiteral("surface"), QStringLiteral("#221E19")},
        {QStringLiteral("surfaceAlt"), QStringLiteral("#2A251F")},
        {QStringLiteral("ink"), QStringLiteral("#E8E1D0")},
        {QStringLiteral("inkSoft"), QStringLiteral("#948A79")},
        {QStringLiteral("line"), QStringLiteral("#3B342C")},
        {QStringLiteral("clay"), QStringLiteral("#D2693A")},
        {QStringLiteral("clayDeep"), QStringLiteral("#B0542E")},
        {QStringLiteral("moss"), QStringLiteral("#76815F")},
        {QStringLiteral("shadowColor"), QStringLiteral("#000000")}};
    theme.fonts = {{QStringLiteral("display"), QStringLiteral("Fraunces")},
                   {QStringLiteral("body"), QStringLiteral("Hanken Grotesk")},
                   {QStringLiteral("mono"), QStringLiteral("IBM Plex Mono")}};
    return theme;
}

ThemeMetadata ThemeMetadata::fromJson(const QJsonObject &json,
                                      const QString &themePath,
                                      QString *error)
{
    ThemeMetadata theme;
    theme.themePath = themePath;
    theme.id = json.value(QStringLiteral("id")).toString().trimmed();
    theme.name = json.value(QStringLiteral("name")).toString().trimmed();
    theme.version = json.value(QStringLiteral("version")).toString().trimmed();
    theme.author = json.value(QStringLiteral("author")).toString().trimmed();
    theme.description =
        json.value(QStringLiteral("description")).toString().trimmed();

    if (theme.id.isEmpty() || theme.name.isEmpty())
    {
        if (error)
            *error = QStringLiteral("theme.json must contain id and name.");
        return {};
    }
    if (theme.id.contains(QStringLiteral("..")) ||
        theme.id.contains(QDir::separator()) ||
        theme.id.contains(QLatin1Char('/')) ||
        theme.id.contains(QLatin1Char('\\')))
    {
        if (error)
            *error = QStringLiteral("Theme id contains invalid path characters.");
        return {};
    }

    const QJsonObject colors = json.value(QStringLiteral("colors")).toObject();
    theme.colorsLight =
        jsonObjectToVariantMap(colors.value(QStringLiteral("light")).toObject());
    theme.colorsDark =
        jsonObjectToVariantMap(colors.value(QStringLiteral("dark")).toObject());
    if (theme.colorsLight.isEmpty() || theme.colorsDark.isEmpty())
    {
        if (error)
            *error = QStringLiteral("theme.json must contain colors.light and colors.dark.");
        return {};
    }

    theme.fonts =
        parseFontMap(json.value(QStringLiteral("fonts")).toObject(), themePath);

    const QJsonObject wallpapers =
        json.value(QStringLiteral("wallpapers")).toObject();
    theme.wallpaperLight = parseWallpaper(
        wallpapers.value(QStringLiteral("light")).toObject(), themePath, error);
    if (error && !error->isEmpty())
        return {};
    theme.wallpaperDark = parseWallpaper(
        wallpapers.value(QStringLiteral("dark")).toObject(), themePath, error);
    if (error && !error->isEmpty())
        return {};

    QString preview = json.value(QStringLiteral("preview")).toString();
    if (preview.trimmed().isEmpty())
    {
        const QDir themeDir(themePath);
        for (const QString &candidate :
             {QStringLiteral("preview.jpg"), QStringLiteral("preview.jpeg"),
              QStringLiteral("preview.png"), QStringLiteral("preview.webp")})
        {
            if (QFileInfo::exists(themeDir.filePath(candidate)))
            {
                preview = candidate;
                break;
            }
        }
    }
    QString previewError;
    theme.previewPath = safeThemeFileUrl(
        themePath, preview,
        {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
         QStringLiteral("webp")},
        &previewError);
    if (!validateThemeImage(theme.previewPath, &previewError))
        theme.previewPath.clear();
    return theme;
}
