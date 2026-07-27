#include "OverwriteTool.h"
#include "ToolWorkdir.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

QString OverwriteTool::description() const
{
    return QStringLiteral(
        "Write `content` to the file at `path`, creating it if missing or "
        "truncating it if it exists. "
        "Use this for full file rewrites; for targeted edits use `edit`.");
}

json OverwriteTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"path",
              {{"type", "string"},
               {"description", "Absolute or workspace-relative file path."}}},
             {"content",
              {{"type", "string"},
               {"description", "The full file content to write."}}},
         }},
        {"required", {"path", "content"}},
    };
}

QString OverwriteTool::execute(const json &args)
{
    const std::string pathStr = args.value("path", std::string());
    const std::string content = args.value("content", std::string());
    if (pathStr.empty())
        return QStringLiteral("Error: `path` is required");

    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);
    const QString path =
        ToolWorkdir::resolvePath(workdir, QString::fromStdString(pathStr));

    // Make sure the parent dir exists (the model may name a path in a subdir
    // that hasn't been created yet).
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QStringLiteral("Error: cannot open %1 for writing").arg(path);

    const QByteArray bytes = QString::fromStdString(content).toUtf8();
    const qint64 n = f.write(bytes);
    f.close();
    if (n != bytes.size())
        return QStringLiteral("Error: short write to %1").arg(path);

    return QStringLiteral("wrote %1 bytes to %2").arg(bytes.size()).arg(path);
}
