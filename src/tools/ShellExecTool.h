#pragma once

#include "Tool.h"

#include <QString>

class ShellExecTool : public Tool
{
  public:
    ShellExecTool(const QString &workspace);

    QString id() const override { return QStringLiteral("shell_exec"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
