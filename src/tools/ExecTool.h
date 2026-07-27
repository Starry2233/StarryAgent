#pragma once

#include "Tool.h"
#include <QString>

// exec — run a shell command and return combined stdout+stderr.
// On Windows the command runs via `cmd.exe /c`; the working directory is the
// workspace. Destructive (arbitrary side effects) → permission required.
class ExecTool : public Tool
{
  public:
    explicit ExecTool(QString workspace) : m_workspace(std::move(workspace)) {}

    QString id() const override { return QStringLiteral("exec"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
