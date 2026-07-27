#pragma once

#include "Tool.h"

class WriteMemoryTool : public Tool
{
  public:
    explicit WriteMemoryTool(QString memoriesDir);

    QString id() const override { return QStringLiteral("write_memory"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return false; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_memoriesDir;
};
