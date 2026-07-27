#include "CliCustomTool.h"
#include "ToolWorkdir.h"

#include <QJsonDocument>
#include <QProcess>

CliCustomTool::CliCustomTool(QString publicId, QString description,
                             nlohmann::json schema, QString command,
                             QStringList args, QString cwd,
                             QProcessEnvironment env, QString inputMode,
                             bool approvalRequired)
    : m_id(std::move(publicId)), m_description(std::move(description)),
      m_schema(std::move(schema)), m_command(std::move(command)),
      m_args(std::move(args)), m_cwd(std::move(cwd)), m_env(std::move(env)),
      m_inputMode(std::move(inputMode)), m_permissionRequired(approvalRequired)
{
}

QString CliCustomTool::execute(const nlohmann::json &args)
{
    if (m_command.trimmed().isEmpty())
        return QStringLiteral("Error: CLI tool command is empty.");

    QProcess process;
    if (!m_env.isEmpty())
        process.setProcessEnvironment(m_env);
    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_cwd);
    if (!workdir.trimmed().isEmpty())
        process.setWorkingDirectory(workdir);

    QStringList finalArgs = m_args;
    const QByteArray payload = QByteArray::fromStdString(args.dump());
    if (m_inputMode == QStringLiteral("argv_json"))
        finalArgs.append(QString::fromUtf8(payload));

    process.start(m_command, finalArgs, QIODevice::ReadWrite);
    if (!process.waitForStarted(10000))
        return QStringLiteral("Error: failed to start CLI tool '%1': %2")
            .arg(m_id, process.errorString());

    if (m_inputMode != QStringLiteral("argv_json"))
    {
        process.write(payload);
        process.closeWriteChannel();
    }

    if (!process.waitForFinished(60000))
    {
        process.kill();
        process.waitForFinished(2000);
        return QStringLiteral("Error: CLI tool '%1' timed out.").arg(m_id);
    }

    const QString stdoutText =
        QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString stderrText =
        QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        return QStringLiteral("Error: CLI tool '%1' failed (exit %2)%3%4")
            .arg(m_id)
            .arg(process.exitCode())
            .arg(stderrText.isEmpty() ? QString() : QStringLiteral(": "))
            .arg(stderrText);
    }
    if (!stdoutText.isEmpty())
        return stdoutText;
    if (!stderrText.isEmpty())
        return stderrText;
    return QStringLiteral("OK");
}
