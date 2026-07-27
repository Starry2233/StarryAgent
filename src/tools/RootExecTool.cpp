#include "RootExecTool.h"

#include "ToolWorkdir.h"

#include <QProcess>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

RootExecTool::RootExecTool(const QString &workspace) : m_workspace(workspace) {}

QString RootExecTool::description() const
{
    return QStringLiteral(
        "Run a shell command with elevated privileges (admin on Windows, root "
        "on "
        "macOS/Linux). Prompts for credentials. Use only for operations that "
        "require "
        "administrator or root access.");
}

json RootExecTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"command",
              {{"type", "string"},
               {"description", "The command to execute as admin/root."}}},
         }},
        {"required", {"command"}},
    };
}

QString RootExecTool::execute(const json &args)
{
    const std::string command = args.value("command", std::string());
    if (command.empty())
        return QStringLiteral("Error: `command` is required");
    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);

    QProcess p;
    p.setWorkingDirectory(workdir);

#ifdef Q_OS_WIN
    // Windows: use `runas` to launch cmd.exe with elevated privileges.
    // Note: this requires a UAC prompt and user confirmation.
    const QString program = QStringLiteral("powershell.exe");
    QStringList shellArgs;
    shellArgs << QStringLiteral("-Command")
              << (QStringLiteral(
                      "Start-Process cmd -Verb runas -ArgumentList '/c ") +
                  QString::fromStdString(command) +
                  QStringLiteral("' -Wait -NoNewWindow"));
#else
    // macOS/Linux: try pkexec first, fall back to sudo -S.
    const QString program = QStringLiteral("pkexec");
    QStringList shellArgs;
    shellArgs << QStringLiteral("/bin/sh") << QStringLiteral("-c")
              << QString::fromStdString(command);
#endif

    p.start(program, shellArgs);
    if (!p.waitForStarted(10000))
        return QStringLiteral("Error: failed to start elevated shell: %1")
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
