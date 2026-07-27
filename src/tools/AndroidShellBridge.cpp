#include "AndroidShellBridge.h"

#ifdef Q_OS_ANDROID
#include <QProcess>

#include <QtCore/QJniObject>

namespace
{
QString escapeForDoubleQuotedShell(const QString &text)
{
    QString escaped = text;
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    escaped.replace(QStringLiteral("$"), QStringLiteral("\\$"));
    escaped.replace(QStringLiteral("`"), QStringLiteral("\\`"));
    return escaped;
}

QString formatProcessResult(QProcess &process)
{
    QString result = QString::fromUtf8(process.readAllStandardOutput());
    const QByteArray err = process.readAllStandardError();
    if (!err.isEmpty())
    {
        if (!result.isEmpty())
            result += QChar('\n');
        result += QStringLiteral("[stderr] ") + QString::fromUtf8(err);
    }
    result += QStringLiteral("\n[exit %1]").arg(process.exitCode());
    return result;
}

QString tryRootShellFallback(const QString &command,
                             const QString &workingDirectory)
{
    QProcess process;
    if (!workingDirectory.isEmpty())
        process.setWorkingDirectory(workingDirectory);
    process.start(QStringLiteral("su"),
                  {QStringLiteral("2000"), QStringLiteral("-c"),
                   QStringLiteral("sh -lc \"%1\"")
                       .arg(escapeForDoubleQuotedShell(command))});
    if (!process.waitForStarted(3000))
        return QStringLiteral(
                   "Error: Shizuku unavailable and `su 2000 -c` is not "
                   "available: %1")
            .arg(process.errorString());

    process.waitForFinished(300000);
    return formatProcessResult(process);
}
} // namespace
#endif

QString AndroidShellBridge::runCommand(const QString &shell,
                                       const QString &command,
                                       const QString &workingDirectory)
{
#ifdef Q_OS_ANDROID
    if (shell != QStringLiteral("sh"))
    {
        return QStringLiteral(
            "Error: Android shell_exec currently supports only `sh`. "
            "`cmd`/`powershell` are desktop-only.");
    }

    QJniObject jResult = QJniObject::callStaticObjectMethod(
        "org/qtproject/example/starryagent/shizuku/ShizukuRunner", "exec",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/"
        "String;",
        QJniObject::fromString(shell).object<jstring>(),
        QJniObject::fromString(command).object<jstring>(),
        QJniObject::fromString(workingDirectory).object<jstring>());

    if (jResult.isValid())
    {
        const QString result = jResult.toString();
        if (!result.startsWith(QStringLiteral("Error: Shizuku unavailable")))
            return result;
    }

    return tryRootShellFallback(command, workingDirectory);
#else
    Q_UNUSED(shell);
    Q_UNUSED(command);
    Q_UNUSED(workingDirectory);
    return QString();
#endif
}
