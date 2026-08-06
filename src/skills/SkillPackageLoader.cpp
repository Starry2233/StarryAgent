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

bool inspectExtractedPackage(const QString &stagingPath,
                            SkillPackageInspection *inspection,
                            QString *error)
{
    const QStringList skillFiles = findSkillFiles(stagingPath);
    if (skillFiles.isEmpty())
    {
        if (error)
            *error = QStringLiteral("Skill package does not contain SKILL.md.");
        return false;
    }

    QList<SkillPackageMetadata> parsed;
    for (const QString &skillFile : skillFiles)
    {
        SkillPackageMetadata metadata;
        if (!parseSkillFile(skillFile, &metadata, error))
            return false;

        metadata.relativePath = QDir::fromNativeSeparators(
            QDir(stagingPath).relativeFilePath(skillFile));
        parsed.append(metadata);
    }

    QList<SkillPackageMetadata> roots;
    QList<SkillPackageMetadata> children;
    for (const SkillPackageMetadata &metadata : parsed)
    {
        const QString relativeSkillFile = QDir::fromNativeSeparators(
            QDir(stagingPath).relativeFilePath(metadata.skillFilePath));
        const QString relativeInstallPath = QDir::fromNativeSeparators(
            QDir(stagingPath).relativeFilePath(metadata.installPath));
        const bool isArchiveRootSkillFile =
            relativeSkillFile.compare(QStringLiteral("SKILL.md"),
                                      Qt::CaseInsensitive) == 0;
        const bool isSingleRootDirectorySkill =
            parsed.size() == 1 &&
            !relativeInstallPath.isEmpty() && relativeInstallPath != QStringLiteral(".") &&
            !relativeInstallPath.contains(QLatin1Char('/'));

        if (isArchiveRootSkillFile || isSingleRootDirectorySkill)
            roots.append(metadata);
        else
            children.append(metadata);
    }

    if (roots.isEmpty())
    {
        if (error)
            *error = QStringLiteral("Skill package is missing a parent SKILL.md.");
        return false;
    }
    if (roots.size() > 1)
    {
        if (error)
            *error = QStringLiteral("Skill package contains multiple parent SKILL.md files.");
        return false;
    }

    if (inspection)
    {
        inspection->parent = roots.first();
        inspection->children = children;
    }
    return true;
}

bool installExtractedSkill(const QString &stagingPath, const QString &skillsRoot,
                           const QString &relativeSkillPath,
                           SkillPackageMetadata *metadata, QString *error)
{
    const QString normalizedRelative = QDir::cleanPath(relativeSkillPath)
                                           .replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalizedRelative.isEmpty() || normalizedRelative == QStringLiteral(".") ||
        normalizedRelative.startsWith(QStringLiteral("../")) ||
        normalizedRelative.contains(QStringLiteral("/../")))
    {
        if (error)
            *error = QStringLiteral("Selected skill path is invalid.");
        return false;
    }

    const QString sourceSkillFile = QDir(stagingPath).filePath(normalizedRelative);
    if (!QFileInfo::exists(sourceSkillFile))
    {
        if (error)
            *error = QStringLiteral("Selected SKILL.md was not found in the package.");
        return false;
    }

    SkillPackageMetadata parsed;
    if (!parseSkillFile(sourceSkillFile, &parsed, error))
        return false;

    parsed.relativePath = normalizedRelative;
    const QString finalPath = QDir(skillsRoot).filePath(parsed.id);
    if (QFileInfo::exists(finalPath) && !removePathRecursively(finalPath))
    {
        if (error)
            *error = QStringLiteral("Failed to replace existing skill install.");
        return false;
    }

    if (!QDir().rename(parsed.installPath, finalPath))
    {
        if (error)
            *error = QStringLiteral("Failed to move skill into storage.");
        return false;
    }

    if (metadata)
    {
        parsed.installPath = finalPath;
        parsed.skillFilePath = QDir(finalPath).filePath(QStringLiteral("SKILL.md"));
        *metadata = parsed;
    }
    return true;
}
} // namespace

bool SkillPackageLoader::installSkillPackage(const QString &archivePath,
                                            const QString &skillsRoot,
                                            SkillPackageMetadata *metadata,
                                            QString *error)
{
    SkillPackageInspection inspection;
    if (!inspectSkillPackage(archivePath, &inspection, error))
        return false;
    return installFromPackage(archivePath, skillsRoot,
                              inspection.parent.relativePath, metadata, error);
}

bool SkillPackageLoader::inspectSkillPackage(const QString &archivePath,
                                             SkillPackageInspection *inspection,
                                             QString *error)
{
    const QString stagingPath =
        QDir(QDir::tempPath()).filePath(QStringLiteral("starryagent-skill-inspect-%1")
                                            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!extractArchive(archivePath, stagingPath, error))
        return false;

    const bool ok = inspectExtractedPackage(stagingPath, inspection, error);
    QDir(stagingPath).removeRecursively();
    return ok;
}

bool SkillPackageLoader::installFromPackage(const QString &archivePath,
                                            const QString &skillsRoot,
                                            const QString &relativeSkillPath,
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

    SkillPackageInspection inspection;
    if (!inspectExtractedPackage(stagingPath, &inspection, error))
    {
        QDir(stagingPath).removeRecursively();
        return false;
    }

    if (!installExtractedSkill(stagingPath, skillsRoot, relativeSkillPath, metadata,
                               error))
    {
        QDir(stagingPath).removeRecursively();
        return false;
    }

    QDir(stagingPath).removeRecursively();
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
