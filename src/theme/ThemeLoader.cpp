#include "ThemeLoader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <QUrl>

#include <archive.h>
#include <archive_entry.h>

namespace
{
constexpr qint64 kMaxArchiveBytes = 50ll * 1024 * 1024;
constexpr qint64 kMaxExtractedBytes = 100ll * 1024 * 1024;
constexpr qint64 kCopyBlockSize = 64ll * 1024;

bool isInsideDirectory(const QString &baseDir, const QString &candidate)
{
    QFileInfo info(candidate);
    QString base = QFileInfo(baseDir).canonicalFilePath();
    if (base.isEmpty())
        base = QDir::cleanPath(QFileInfo(baseDir).absoluteFilePath());
    QString path = info.exists() ? info.canonicalFilePath()
                                 : QDir::cleanPath(info.absoluteFilePath());
    if (base.isEmpty())
        return false;

    base = QDir::cleanPath(base).replace(QLatin1Char('\\'), QLatin1Char('/'));
    path = QDir::cleanPath(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    const QString baseWithSlash = base + QLatin1Char('/');
    return path.compare(base, Qt::CaseInsensitive) == 0 ||
           path.startsWith(baseWithSlash, Qt::CaseInsensitive);
}

QString normalizedArchiveEntryPath(const QString &path)
{
    QString normalized =
        QDir::cleanPath(path).replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (normalized.startsWith(QStringLiteral("./")))
        normalized.remove(0, 2);
    return normalized;
}

bool isSafeArchiveEntryPath(const QString &path)
{
    if (path.isEmpty() || path == QStringLiteral(".") ||
        QDir::isAbsolutePath(path))
        return false;
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return !parts.contains(QStringLiteral(".."));
}

QString localPathFromUrlOrPath(const QString &value)
{
    const QUrl url(value);
    if (url.isLocalFile())
        return url.toLocalFile();
    return value;
}

} // namespace

bool ThemeLoader::extractTheme(const QString &archivePath,
                               const QString &destDir, QString *error)
{
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile())
    {
        if (error)
            *error = QStringLiteral("Theme package does not exist.");
        return false;
    }
    if (archiveInfo.size() > kMaxArchiveBytes)
    {
        if (error)
            *error = QStringLiteral("Theme package exceeds 50MB.");
        return false;
    }

    QDir parent(QFileInfo(destDir).absolutePath());
    parent.mkpath(QStringLiteral("."));
    if (QFileInfo::exists(destDir))
        QDir(destDir).removeRecursively();
    QDir().mkpath(destDir);

    if (!extractTarZst(archivePath, destDir, error))
    {
        QDir(destDir).removeRecursively();
        return false;
    }
    return true;
}

ThemeMetadata ThemeLoader::loadTheme(const QString &themePath, QString *error)
{
    const QString jsonPath =
        QDir(themePath).filePath(QStringLiteral("theme.json"));
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error)
            *error = QStringLiteral("theme.json not found.");
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
    {
        if (error)
            *error = QStringLiteral("theme.json is not a JSON object.");
        return {};
    }
    return ThemeMetadata::fromJson(doc.object(), themePath, error);
}

bool ThemeLoader::loadThemeFonts(const ThemeMetadata &theme, QString *error)
{
    bool ok = true;
    for (const QString &key :
         {QStringLiteral("display"), QStringLiteral("body"), QStringLiteral("mono")})
    {
        const QVariant value = theme.fonts.value(key);
        if (!value.canConvert<QVariantMap>())
            continue;
        const QVariantMap font = value.toMap();
        const QString source = font.value(QStringLiteral("source")).toString();
        if (source.isEmpty())
            continue;
        const int id =
            QFontDatabase::addApplicationFont(localPathFromUrlOrPath(source));
        if (id < 0)
        {
            ok = false;
            if (error)
                *error = QStringLiteral("Failed to load theme font: %1")
                             .arg(source);
        }
    }
    return ok;
}

bool ThemeLoader::extractTarZst(const QString &archivePath,
                                const QString &destDir, QString *error)
{
    archive *input = archive_read_new();
    archive_read_support_format_tar(input);
    archive_read_support_filter_zstd(input);
    archive_read_support_filter_gzip(input);
    archive_read_support_filter_xz(input);

    const QByteArray pathBytes = QFile::encodeName(archivePath);
    if (archive_read_open_filename(input, pathBytes.constData(),
                                   size_t(kCopyBlockSize)) != ARCHIVE_OK)
    {
        if (error)
            *error = QString::fromUtf8(archive_error_string(input));
        archive_read_free(input);
        return false;
    }

    qint64 extractedBytes = 0;
    archive_entry *entry = nullptr;
    while (archive_read_next_header(input, &entry) == ARCHIVE_OK)
    {
        const QString rawEntryPath =
            QString::fromUtf8(archive_entry_pathname(entry));
        const QString entryPath = normalizedArchiveEntryPath(rawEntryPath);
        if (entryPath == QStringLiteral("."))
        {
            archive_read_data_skip(input);
            continue;
        }
        if (!isSafeArchiveEntryPath(entryPath))
        {
            if (error)
                *error = QStringLiteral("Theme package contains unsafe path: %1")
                             .arg(rawEntryPath);
            archive_read_free(input);
            return false;
        }

        const QString outputPath = QDir(destDir).filePath(entryPath);
        const QFileInfo outputInfo(outputPath);
        QDir().mkpath(outputInfo.absolutePath());
        if (!isInsideDirectory(destDir, outputPath))
        {
            if (error)
                *error = QStringLiteral("Theme package escapes destination: %1")
                             .arg(rawEntryPath);
            archive_read_free(input);
            return false;
        }

        const auto type = archive_entry_filetype(entry);
        if (type == AE_IFDIR)
        {
            QDir().mkpath(outputPath);
            continue;
        }
        if (type != AE_IFREG)
        {
            archive_read_data_skip(input);
            continue;
        }

        QFile out(outputPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            if (error)
                *error = QStringLiteral("Failed to write extracted file: %1")
                             .arg(outputPath);
            archive_read_free(input);
            return false;
        }

        const void *buff = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while (true)
        {
            const int rc = archive_read_data_block(input, &buff, &size, &offset);
            if (rc == ARCHIVE_EOF)
                break;
            if (rc != ARCHIVE_OK)
            {
                if (error)
                    *error = QString::fromUtf8(archive_error_string(input));
                archive_read_free(input);
                return false;
            }
            extractedBytes += qint64(size);
            if (extractedBytes > kMaxExtractedBytes)
            {
                if (error)
                    *error = QStringLiteral(
                        "Theme package expands beyond 100MB.");
                archive_read_free(input);
                return false;
            }
            out.write(static_cast<const char *>(buff), qint64(size));
        }
        out.close();
    }

    archive_read_close(input);
    archive_read_free(input);
    return true;
}
