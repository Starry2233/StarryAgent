#pragma once

#include "Tool.h"

class SkillManager;

class LoadSkillTool : public Tool
{
  public:
    explicit LoadSkillTool(SkillManager *manager);

    QString id() const override { return QStringLiteral("load_skill"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return false; }
    QString execute(const nlohmann::json &args) override;

  private:
    SkillManager *m_manager;
};
