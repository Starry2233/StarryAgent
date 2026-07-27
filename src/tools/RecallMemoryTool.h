#pragma once

#include "Tool.h"

class RecallMemoryTool : public Tool
{
  public:
    explicit RecallMemoryTool(QString memoriesDir);

    QString id() const override { return QStringLiteral("recall_memory"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return false; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_memoriesDir;
};
