#pragma once

#include "Tool.h"

#include <QString>

// Execute a command with elevated privileges. On Windows, uses `runas`.
// On macOS/Linux, uses `pkexec` (or `sudo` as fallback).
class RootExecTool : public Tool
{
  public:
    RootExecTool(const QString &workspace);

    QString id() const override { return QStringLiteral("root_exec"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
