#include "SkillPackageLoader.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUuid>

#include <archive.h>
#include <archive_entry.h>

namespace
{
constexpr qint64 kMaxArchiveBytes = 50ll * 1024 * 1024;
constexpr qint64 kMaxExtractedBytes = 100ll * 1024 * 1024;
constexpr qint64 kCopyBlockSize = 64ll * 1024;

bool isInsideDirectory(const QString &baseDir, const QString &candidate)
{
    QString base = QDir::cleanPath(QFileInfo(baseDir).absoluteFilePath());
    QString path = QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());
    if (base.isEmpty() || path.isEmpty())
        return false;

    base = base.replace(QLatin1Char('\\'), QLatin1Char('/'));
    path = path.replace(QLatin1Char('\\'), QLatin1Char('/'));
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

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream stream(&file);
    return stream.readAll();
}

QString normalizeNewlines(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString stripQuotes(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2)
    {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\'') && last == QLatin1Char('\'')) ||
            (first == QLatin1Char('"') && last == QLatin1Char('"')))
        {
            value = value.mid(1, value.size() - 2);
        }
    }
    return value.trimmed();
}

QString sanitizeInstallId(QString value)
{
    value = value.trimmed().toLower();
    QString out;
    out.reserve(value.size());
    bool lastWasDash = false;
    for (const QChar ch : value)
    {
        if (ch.isLetterOrNumber())
        {
            out.append(ch);
            lastWasDash = false;
            continue;
        }
        if ((ch == QLatin1Char('-') || ch == QLatin1Char('_') ||
             ch == QLatin1Char(' ') || ch == QLatin1Char('.')) &&
            !lastWasDash)
        {
            out.append(QLatin1Char('-'));
            lastWasDash = true;
        }
    }
    while (out.startsWith(QLatin1Char('-')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('-')))
        out.chop(1);
    return out;
}

bool parseSkillFile(const QString &skillMdPath, SkillPackageMetadata *metadata,
                    QString *error)
{
    const QString raw = readTextFile(skillMdPath);
    if (raw.isEmpty())
    {
        if (error)
            *error = QStringLiteral("SKILL.md is empty or unreadable.");
        return false;
    }

    const QString normalized = normalizeNewlines(raw);
    QString id = QFileInfo(QFileInfo(skillMdPath).dir().dirName()).baseName();
    QString name;
    QString description;

    if (normalized.startsWith(QStringLiteral("---\n")))
    {
        const QStringList lines = normalized.split(QLatin1Char('\n'));
        int closingFence = -1;
        for (int i = 1; i < lines.size(); ++i)
        {
            if (lines.at(i).trimmed() == QStringLiteral("---"))
            {
                closingFence = i;
                break;
            }
        }
        if (closingFence > 0)
        {
            const QStringList frontmatterLines = lines.mid(1, closingFence - 1);
            for (const QString &line : frontmatterLines)
            {
                const QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
                    continue;
                const int colon = trimmed.indexOf(QLatin1Char(':'));
                if (colon <= 0)
                    continue;
                const QString key = trimmed.left(colon).trimmed();
                const QString value = stripQuotes(trimmed.mid(colon + 1));
                if (key == QStringLiteral("name") && !value.isEmpty())
                    id = value;
                else if (key == QStringLiteral("description"))
                    description = value;
            }
        }
    }

    const QString sanitizedId = sanitizeInstallId(id);
    if (sanitizedId.isEmpty())
    {
        if (error)
            *error = QStringLiteral("SKILL.md does not define a valid skill name.");
        return false;
    }

    name = id.trimmed();
    if (metadata)
    {
        metadata->id = sanitizedId;
        metadata->name = name.isEmpty() ? sanitizedId : name;
        metadata->description = description;
        metadata->skillFilePath = skillMdPath;
        metadata->installPath = QFileInfo(skillMdPath).dir().absolutePath();
    }
    return true;
}

QStringList findSkillFiles(const QString &root)
{
    QStringList found;
    QDirIterator it(root, {QStringLiteral("SKILL.md")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
        found.append(QFileInfo(it.next()).absoluteFilePath());
    found.sort();
    return found;
}

bool removePathRecursively(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists())
        return true;
    if (info.isDir())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}
} // namespace

bool SkillPackageLoader::installSkillPackage(const QString &archivePath,
                                            const QString &skillsRoot,
                                            SkillPackageMetadata *metadata,
                                            QString *error)
{
    if (skillsRoot.trimmed().isEmpty())
    {
        if (error)
            *error = QStringLiteral("Skill storage is not ready.");
        return false;
    }

    QDir root(skillsRoot);
    root.mkpath(QStringLiteral("."));
    const QString stagingPath =
        root.filePath(QStringLiteral(".install-%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!extractArchive(archivePath, stagingPath, error))
        return false;

    const QStringList skillFiles = findSkillFiles(stagingPath);
    if (skillFiles.size() != 1)
    {
        QDir(stagingPath).removeRecursively();
        if (error)
        {
            *error = skillFiles.isEmpty()
                         ? QStringLiteral("Skill package does not contain SKILL.md.")
                         : QStringLiteral("Skill package contains multiple SKILL.md files.");
        }
        return false;
    }

    SkillPackageMetadata parsed;
    if (!parseSkillFile(skillFiles.first(), &parsed, error))
    {
        QDir(stagingPath).removeRecursively();
        return false;
    }

    const QString finalPath = root.filePath(parsed.id);
    if (QFileInfo::exists(finalPath) && !removePathRecursively(finalPath))
    {
        QDir(stagingPath).removeRecursively();
        if (error)
            *error = QStringLiteral("Failed to replace existing skill install.");
        return false;
    }

    if (!QDir().rename(parsed.installPath, finalPath))
    {
        QDir(stagingPath).removeRecursively();
        if (error)
            *error = QStringLiteral("Failed to move skill into storage.");
        return false;
    }

    QDir(stagingPath).removeRecursively();
    if (metadata)
    {
        parsed.installPath = finalPath;
        parsed.skillFilePath = QDir(finalPath).filePath(QStringLiteral("SKILL.md"));
        *metadata = parsed;
    }
    return true;
}

bool SkillPackageLoader::extractArchive(const QString &archivePath,
                                        const QString &destDir, QString *error)
{
    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile())
    {
        if (error)
            *error = QStringLiteral("Skill package does not exist.");
        return false;
    }
    if (archiveInfo.size() > kMaxArchiveBytes)
    {
        if (error)
            *error = QStringLiteral("Skill package exceeds 50MB.");
        return false;
    }

    QDir parent(QFileInfo(destDir).absolutePath());
    parent.mkpath(QStringLiteral("."));
    if (QFileInfo::exists(destDir))
        QDir(destDir).removeRecursively();
    QDir().mkpath(destDir);

    archive *input = archive_read_new();
    archive_read_support_format_tar(input);
    archive_read_support_format_zip(input);
    archive_read_support_filter_gzip(input);
    archive_read_support_filter_none(input);

    const QByteArray pathBytes = QFile::encodeName(archivePath);
    if (archive_read_open_filename(input, pathBytes.constData(),
                                   size_t(kCopyBlockSize)) != ARCHIVE_OK)
    {
        if (error)
            *error = QString::fromUtf8(archive_error_string(input));
        archive_read_free(input);
        QDir(destDir).removeRecursively();
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
                *error = QStringLiteral("Skill package contains unsafe path: %1")
                             .arg(rawEntryPath);
            archive_read_free(input);
            QDir(destDir).removeRecursively();
            return false;
        }

        const QString outputPath = QDir(destDir).filePath(entryPath);
        const QFileInfo outputInfo(outputPath);
        QDir().mkpath(outputInfo.absolutePath());
        if (!isInsideDirectory(destDir, outputPath))
        {
            if (error)
                *error = QStringLiteral("Skill package escapes destination: %1")
                             .arg(rawEntryPath);
            archive_read_free(input);
            QDir(destDir).removeRecursively();
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
            QDir(destDir).removeRecursively();
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
                QDir(destDir).removeRecursively();
                return false;
            }
            extractedBytes += qint64(size);
            if (extractedBytes > kMaxExtractedBytes)
            {
                if (error)
                    *error = QStringLiteral("Skill package expands beyond 100MB.");
                archive_read_free(input);
                QDir(destDir).removeRecursively();
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
