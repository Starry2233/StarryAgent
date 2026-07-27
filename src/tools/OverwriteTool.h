#pragma once

#include "Tool.h"
#include <QString>

// overwrite — write `content` to `path`, creating or truncating the file.
// Destructive → permission required.
class OverwriteTool : public Tool
{
  public:
    explicit OverwriteTool(QString workspace)
        : m_workspace(std::move(workspace))
    {
    }

    QString id() const override { return QStringLiteral("overwrite"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
