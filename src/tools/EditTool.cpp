#include "EditTool.h"
#include "ToolWorkdir.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

QString EditTool::description() const
{
    return QStringLiteral(
        "Replace the first occurrence of `oldText` with "
        "`newText` in the file at `path`. "
        "Use this for targeted edits to existing files. For a "
        "full rewrite, use `overwrite`.");
}

json EditTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"path",
              {{"type", "string"},
               {"description", "Absolute or workspace-relative file path."}}},
             {"oldText",
              {{"type", "string"},
               {"description",
                "The exact text to find (first occurrence replaced)."}}},
             {"newText",
              {{"type", "string"}, {"description", "The replacement text."}}},
         }},
        {"required", {"path", "oldText", "newText"}},
    };
}

QString EditTool::execute(const json &args)
{
    const std::string pathStr = args.value("path", std::string());
    const std::string oldText = args.value("oldText", std::string());
    const std::string newText = args.value("newText", std::string());
    if (pathStr.empty())
        return QStringLiteral("Error: `path` is required");

    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);
    const QString path =
        ToolWorkdir::resolvePath(workdir, QString::fromStdString(pathStr));
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("Error: cannot open %1 for reading").arg(path);

    const QString content = QTextStream(&f).readAll();
    f.close();

    const QString oldQ = QString::fromStdString(oldText);
    const QString newQ = QString::fromStdString(newText);
    const int idx = content.indexOf(oldQ);
    if (idx < 0)
        return QStringLiteral("Error: `oldText` not found in %1").arg(path);

    QString out = content;
    out.replace(idx, oldQ.size(), newQ);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return QStringLiteral("Error: cannot open %1 for writing").arg(path);
    QTextStream(&f) << out;
    f.close();

    return QStringLiteral("edited %1 (replaced %2 chars at offset %3)")
        .arg(path)
        .arg(oldQ.size())
        .arg(idx);
}
