#pragma once

#include <QDir>
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

inline QString resolvePath(const QString &workdir, const QString &path)
{
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);
    return QDir::cleanPath(QDir(workdir).filePath(path));
}

} // namespace ToolWorkdir
