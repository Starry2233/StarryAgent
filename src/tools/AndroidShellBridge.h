#pragma once

#include <QString>

class AndroidShellBridge
{
  public:
    static QString runCommand(const QString &shell, const QString &command,
                              const QString &workingDirectory);
};
