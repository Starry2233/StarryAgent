#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>

#include <nlohmann/json.hpp>

namespace ToolWorkdir
{

inline QString effectiveWorkdir(const nlohmann::json &args,
                                const QString &fallback)
{
    const std::string raw = args.value("__workdir", std::string());
    if (raw.empty())
        return fallback;
    const QString requested = QString::fromStdString(raw).trimmed();
    return requested.isEmpty() ? fallback : QDir::cleanPath(requested);
}

inline bool isAndroidAppPrivatePath(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path).trimmed();
    return cleaned.startsWith(QStringLiteral("/data/user/0/")) ||
           cleaned.startsWith(QStringLiteral("/data/data/"));
}

inline bool isUsableDirectory(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;
    const QFileInfo info(path);
    return info.exists() && info.isDir();
}

inline QString androidShellFallbackWorkdir(const QString &preferred)
{
    const QString cleanedPreferred = QDir::cleanPath(preferred).trimmed();
    if (isUsableDirectory(cleanedPreferred) &&
        !isAndroidAppPrivatePath(cleanedPreferred))
    {
        return cleanedPreferred;
    }

    const QStringList candidates = {
        QStringLiteral("/sdcard"),
        QStringLiteral("/storage/emulated/0"),
        QStringLiteral("/data/local/tmp"),
        QStringLiteral("/")};
    for (const QString &candidate : candidates)
    {
        if (isUsableDirectory(candidate))
            return candidate;
    }
    return QStringLiteral("/");
}

inline QString effectiveAndroidShellWorkdir(const nlohmann::json &args,
                                            const QString &fallback)
{
    return androidShellFallbackWorkdir(effectiveWorkdir(args, fallback));
}

inline QString resolvePath(const QString &workdir, const QString &path)
{
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);
    return QDir::cleanPath(QDir(workdir).filePath(path));
}

} // namespace ToolWorkdir
