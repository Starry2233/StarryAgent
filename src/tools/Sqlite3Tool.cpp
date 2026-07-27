#include "Sqlite3Tool.h"
#include "ToolWorkdir.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

Sqlite3Tool::Sqlite3Tool(const QString &workspace) : m_workspace(workspace) {}

QString Sqlite3Tool::description() const
{
    return QStringLiteral(
        "Run a single SQL query against a SQLite database "
        "file. Returns the query result "
        "in tab-separated rows. Use for data analysis, schema "
        "inspection, or data mutation.");
}

json Sqlite3Tool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"path",
              {{"type", "string"},
               {"description", "Path to the SQLite database file (relative to "
                               "workspace or absolute)."}}},
             {"query",
              {{"type", "string"},
               {"description", "A single SQL query to execute."}}},
         }},
        {"required", {"path", "query"}},
    };
}

QString Sqlite3Tool::execute(const json &args)
{
    std::string pathStr = args.value("path", std::string());
    const std::string query = args.value("query", std::string());
    if (pathStr.empty())
        return QStringLiteral("Error: `path` is required");
    if (query.empty())
        return QStringLiteral("Error: `query` is required");
    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);

    // Resolve relative paths to workspace.
    QString path = QString::fromStdString(pathStr);
    if (!QFile::exists(path) && !QFileInfo(path).isAbsolute())
        path = ToolWorkdir::resolvePath(workdir, path);

    // Invoke the sqlite3 CLI.
    QProcess p;
    p.setWorkingDirectory(workdir);
#ifdef Q_OS_WIN
    const QString program = QStringLiteral("sqlite3.exe");
#else
    const QString program = QStringLiteral("sqlite3");
#endif
    p.start(program, {path, QString::fromStdString(query)});

    if (!p.waitForStarted(10000))
        return QStringLiteral("Error: sqlite3 not found on PATH");

    p.waitForFinished(30000);

    const QByteArray out = p.readAllStandardOutput();
    const QByteArray err = p.readAllStandardError();
    const int code = p.exitCode();

    QString result = QString::fromUtf8(out);
    if (!err.isEmpty())
    {
        if (!result.isEmpty())
            result += QChar('\n');
        result += QStringLiteral("[stderr] ") + QString::fromUtf8(err);
    }
    result += QStringLiteral("\n[exit %1]").arg(code);
    return result;
}
