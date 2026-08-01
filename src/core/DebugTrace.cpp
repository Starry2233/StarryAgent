#include "DebugTrace.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QMutex>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <android/log.h>
#endif

bool DebugTrace::s_verboseEnabled = false;
QString DebugTrace::s_logType;

namespace
{
QMutex &logMutex()
{
    static QMutex mutex;
    return mutex;
}

const char *msgTypeName(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "CRIT";
    case QtFatalMsg:
        return "FATAL";
    }
    return "LOG";
}

void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &message)
{
    QMutexLocker locker(&logMutex());
    QTextStream ts(stderr);
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString category = context.category
                                 ? QString::fromUtf8(context.category)
                                 : QStringLiteral("qt");
    const QString file =
        context.file ? QFileInfo(QString::fromUtf8(context.file)).fileName()
                     : QStringLiteral("-");
    ts << '[' << timestamp << "] [" << msgTypeName(type) << "] [" << category
       << "] "
       << "[tid " << reinterpret_cast<quintptr>(QThread::currentThreadId())
       << "] " << file << ':' << context.line << ' ' << message << Qt::endl;
    ts.flush();
#ifdef Q_OS_ANDROID
    __android_log_print(type == QtWarningMsg ? ANDROID_LOG_WARN
                                             : type == QtCriticalMsg ||
                                                       type == QtFatalMsg
                                                   ? ANDROID_LOG_ERROR
                                                   : ANDROID_LOG_INFO,
                        "StarryAgent", "%s", qPrintable(message));
#endif
    if (type == QtFatalMsg)
        abort();
}
} // namespace

void DebugTrace::configureFromArgs(const QStringList &args)
{
    s_logType.clear();
    s_verboseEnabled = false;
    for (int i = 0; i < args.size(); ++i)
    {
        const QString &arg = args.at(i);
        if (arg == QStringLiteral("--logtype") && i + 1 < args.size())
        {
            s_logType = args.at(i + 1).trimmed().toLower();
        }
        else if (arg.startsWith(QStringLiteral("--logtype=")))
        {
            s_logType = arg.mid(QStringLiteral("--logtype=").size())
                            .trimmed()
                            .toLower();
        }
        else
        {
            continue;
        }
        s_verboseEnabled = (s_logType == QStringLiteral("verbose"));
    }
}

bool DebugTrace::verboseEnabled() { return s_verboseEnabled; }

QString DebugTrace::logType() { return s_logType; }

void DebugTrace::installMessageHandler()
{
    qInstallMessageHandler(messageHandler);
}

void DebugTrace::applyVerboseQtEnvironment()
{
    if (!s_verboseEnabled)
        return;

    qputenv(
        "QT_LOGGING_RULES",
        QByteArrayLiteral("*.debug=true;qt.*.debug=true;qt.qml.debug=true;qt."
                          "qml.*=true;qt.quick.*=true;qt.scenegraph.*=true;qt."
                          "rhi.*=true"));
    qputenv("QT_FORCE_STDERR_LOGGING", QByteArrayLiteral("1"));
    qputenv("QSG_INFO", QByteArrayLiteral("1"));
    qputenv("QSG_RENDERER_DEBUG", QByteArrayLiteral("render"));
    qputenv("QSG_RHI_PROFILE", QByteArrayLiteral("1"));
    qputenv("QML_IMPORT_TRACE", QByteArrayLiteral("1"));
}

void DebugTrace::setupAutoExit(QCoreApplication *app, int milliseconds)
{
    if (!s_verboseEnabled || !app)
        return;

    QTimer::singleShot(
        milliseconds, app,
        [milliseconds]
        {
            DebugTrace::verbose(
                "app", QStringLiteral("auto-exit timer fired after %1 ms")
                           .arg(milliseconds));
            QCoreApplication::quit();
        });
}

void DebugTrace::verbose(const char *module, const QString &message)
{
    if (!s_verboseEnabled)
        return;

    qInfo().noquote() << QStringLiteral("[verbose:%1] %2")
                             .arg(QString::fromUtf8(module), message);
}
