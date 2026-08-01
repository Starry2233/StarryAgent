#include "AndroidShellBridge.h"

#ifdef Q_OS_ANDROID
#include <QJsonDocument>
#include <QJsonObject>
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

QString normalizeRishShell(const QString &shell)
{
    const QString trimmed = shell.trimmed();
    if (trimmed == QStringLiteral("rish"))
        return QStringLiteral("sh");
    if (!trimmed.startsWith(QStringLiteral("rish ")))
        return QString();

    const QString requested = trimmed.mid(5).trimmed();
    if (requested.isEmpty())
        return QStringLiteral("sh");
    if (requested == QStringLiteral("sh"))
        return requested;
    return QString();
}

QString formatProcessResult(const QString &stdoutText,
                            const QString &stderrText, int exitCode)
{
    QString result = stdoutText;
    if (!stderrText.isEmpty())
    {
        if (!result.isEmpty())
            result += QChar('\n');
        result += QStringLiteral("[stderr] ") + stderrText;
    }
    result += QStringLiteral("\n[exit %1]").arg(exitCode);
    return result;
}

QString formatProcessResult(QProcess &process)
{
    return formatProcessResult(QString::fromUtf8(process.readAllStandardOutput()),
                               QString::fromUtf8(process.readAllStandardError()),
                               process.exitCode());
}

QString jsonFieldString(const QJsonObject &object, const char *key)
{
    return object.value(QLatin1String(key)).toString();
}

int jsonFieldInt(const QJsonObject &object, const char *key, int fallback)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toInt(fallback) : fallback;
}

QString formatBridgeJsonResult(const QJsonObject &object)
{
    return formatProcessResult(jsonFieldString(object, "stdout"),
                               jsonFieldString(object, "stderr"),
                               jsonFieldInt(object, "exitCode", -1));
}

QString formatBridgeError(const QJsonObject &object)
{
    const QString status = jsonFieldString(object, "status");
    const QString message = jsonFieldString(object, "message");
    if (status == QStringLiteral("permission_required"))
    {
        return QStringLiteral("Error: Shizuku permission not granted. %1")
            .arg(message.isEmpty()
                     ? QStringLiteral("Grant permission in Shizuku and retry.")
                     : message);
    }
    if (status == QStringLiteral("shizuku_unavailable"))
        return QString();
    if (status == QStringLiteral("invalid_args"))
        return QStringLiteral("Error: %1").arg(message);
    if (!message.isEmpty())
        return QStringLiteral("Error: %1").arg(message);
    return QStringLiteral("Error: Android shell bridge failed (%1)").arg(status);
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
    const QString normalizedShell = normalizeRishShell(shell);
    if (normalizedShell.isEmpty())
    {
        return QStringLiteral(
            "Error: Android Shizuku shell_exec requires `rish` or `rish sh`.");
    }

    QJniObject jResult = QJniObject::callStaticObjectMethod(
        "moe/starry2233/StarryAgent/shizuku/ShizukuRunner", "exec",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/"
        "String;",
        QJniObject::fromString(normalizedShell).object<jstring>(),
        QJniObject::fromString(command).object<jstring>(),
        QJniObject::fromString(workingDirectory).object<jstring>());

    if (jResult.isValid())
    {
        const QString result = jResult.toString();
        const QJsonDocument document = QJsonDocument::fromJson(result.toUtf8());
        if (!document.isNull() && document.isObject())
        {
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("ok")).toBool())
                return formatBridgeJsonResult(object);

            const QString status = jsonFieldString(object, "status");
            if (status == QStringLiteral("shizuku_unavailable"))
                return tryRootShellFallback(command, workingDirectory);
            return formatBridgeError(object);
        }

        return QStringLiteral("Error: Invalid Shizuku bridge response.");
    }

    return tryRootShellFallback(command, workingDirectory);
#else
    Q_UNUSED(shell);
    Q_UNUSED(command);
    Q_UNUSED(workingDirectory);
    return QString();
#endif
}
