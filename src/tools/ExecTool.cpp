#include "ExecTool.h"

#include "AndroidShellBridge.h"
#include "ToolWorkdir.h"

#include <QProcess>
#include <QStandardPaths>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

QString ExecTool::description() const
{
    return QStringLiteral(
        "Run a shell command and return its combined stdout+stderr. "
        "On Android this uses the native `sh` shell, not Shizuku. "
        "On Windows the command runs via `cmd.exe /c`. Use for build steps, "
        "git operations, and other CLI tasks. The working directory is the "
        "workspace.");
}

json ExecTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"command",
              {{"type", "string"},
               {"description", "The shell command to execute."}}},
         }},
        {"required", {"command"}},
    };
}

QString ExecTool::execute(const json &args)
{
    const std::string command = args.value("command", std::string());
    if (command.empty())
        return QStringLiteral("Error: `command` is required");
    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);

    QProcess p;
    p.setWorkingDirectory(workdir);
    // On Windows, route through cmd.exe so pipes/redirects/builtins work.
    // On other platforms (future), invoke the shell directly.
    QStringList progArgs;
#ifdef Q_OS_WIN
    const QString program = QStringLiteral("cmd.exe");
    progArgs << QStringLiteral("/c") << QString::fromStdString(command);
#else
    const QString program = QStringLiteral("/bin/sh");
    progArgs << QStringLiteral("-c") << QString::fromStdString(command);
#endif

    p.start(program, progArgs);
    const bool ok = p.waitForStarted(10000);
    if (!ok)
        return QStringLiteral("Error: failed to start command: %1")
            .arg(p.errorString());

    // 5-minute ceiling — long-running commands should stream (future).
    p.waitForFinished(300000);

    const QByteArray out = p.readAllStandardOutput();
    const QByteArray err = p.readAllStandardError();
    const int code = p.exitCode();

    QString result;
    result += QString::fromUtf8(out);
    if (!err.isEmpty())
    {
        if (!result.isEmpty())
            result += QChar('\n');
        result += QStringLiteral("[stderr] ") + QString::fromUtf8(err);
    }
    result += QStringLiteral("\n[exit %1]").arg(code);
    return result;
}
