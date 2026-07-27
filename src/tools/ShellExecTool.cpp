#include "ShellExecTool.h"

#include "AndroidShellBridge.h"
#include "ToolWorkdir.h"

#include <QProcess>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

ShellExecTool::ShellExecTool(const QString &workspace) : m_workspace(workspace)
{
}

QString ShellExecTool::description() const
{
    return QStringLiteral(
        "Run a shell command with explicit shell selection. "
        "Use `sh` for POSIX sh, `cmd` for Windows cmd.exe, or "
        "`powershell` for pwsh. "
        "Unlike `exec`, you choose the shell explicitly.");
}

json ShellExecTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"shell",
              {{"type", "string"},
               {"enum", {"sh", "cmd", "powershell"}},
               {"description", "Shell to use: sh/cmd/powershell."}}},
             {"command",
              {{"type", "string"}, {"description", "The command to execute."}}},
         }},
        {"required", {"shell", "command"}},
    };
}

QString ShellExecTool::execute(const json &args)
{
    const std::string shellStr = args.value("shell", std::string());
    const std::string command = args.value("command", std::string());
    if (command.empty())
        return QStringLiteral("Error: `command` is required");
    if (shellStr.empty())
        return QStringLiteral("Error: `shell` is required (sh/cmd/powershell)");

    const QString shell = QString::fromStdString(shellStr);
    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);

#ifdef Q_OS_ANDROID
    return AndroidShellBridge::runCommand(
        shell, QString::fromStdString(command), workdir);
#endif

    QProcess p;
    p.setWorkingDirectory(workdir);

    if (shell == QStringLiteral("cmd") || shell == QStringLiteral("powershell"))
    {
        const QString program = (shell == QStringLiteral("cmd"))
                                    ? QStringLiteral("cmd.exe")
                                    : QStringLiteral("powershell.exe");
        const QString shellArg = (shell == QStringLiteral("cmd"))
                                     ? QStringLiteral("/c")
                                     : QStringLiteral("-Command");
        p.start(program, {shellArg, QString::fromStdString(command)});
    }
    else
    {
        // sh — on Windows use Git's sh, on Unix use /bin/sh
#ifdef Q_OS_WIN
        const QString program = QStringLiteral("sh.exe");
        p.start(program,
                {QStringLiteral("-c"), QString::fromStdString(command)});
#else
        const QString program = QStringLiteral("/bin/sh");
        p.start(program,
                {QStringLiteral("-c"), QString::fromStdString(command)});
#endif
    }

    if (!p.waitForStarted(10000))
        return QStringLiteral("Error: failed to start shell: %1")
            .arg(p.errorString());

    p.waitForFinished(300000);

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
