#pragma once

#include <QString>
#include <QStringList>

#include "backend/StarryAgentBackendGlobal.h"

class QCoreApplication;

class STARRYAGENT_BACKEND_EXPORT DebugTrace
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
