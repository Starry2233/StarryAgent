#pragma once

#include "Tool.h"
#include <QString>

// edit — replace the first occurrence of `oldText` with `newText` in `path`.
// Destructive (writes back to disk) → permission required.
class EditTool : public Tool
{
  public:
    explicit EditTool(QString workspace) : m_workspace(std::move(workspace)) {}

    QString id() const override { return QStringLiteral("edit"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
