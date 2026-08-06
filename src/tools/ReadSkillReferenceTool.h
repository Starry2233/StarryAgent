#pragma once

#include "Tool.h"

class SkillManager;

class ReadSkillReferenceTool : public Tool
{
  public:
    explicit ReadSkillReferenceTool(SkillManager *manager);

    QString id() const override
    {
        return QStringLiteral("read_skill_reference");
    }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return false; }
    QString execute(const nlohmann::json &args) override;

  private:
    SkillManager *m_manager;
};
