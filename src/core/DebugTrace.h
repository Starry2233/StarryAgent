#pragma once

#include <QString>
#include <QStringList>

class QCoreApplication;

class DebugTrace
{
  public:
    static void configureFromArgs(const QStringList &args);
    static bool verboseEnabled();
    static QString logType();
    static void installMessageHandler();
    static void applyVerboseQtEnvironment();
    static void setupAutoExit(QCoreApplication *app, int milliseconds);
    static void verbose(const char *module, const QString &message);

  private:
    static bool s_verboseEnabled;
    static QString s_logType;
};
