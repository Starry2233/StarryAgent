#include "LoadSkillTool.h"

#include "skills/SkillManager.h"

using json = nlohmann::json;

LoadSkillTool::LoadSkillTool(SkillManager *manager) : m_manager(manager) {}

QString LoadSkillTool::description() const
{
    return QStringLiteral("Load a skill's full SKILL.md content and list its auxiliary reference files.");
}

json LoadSkillTool::schema() const
{
    return {{"type", "object"},
            {"properties",
             {{"skill_id",
               {{"type", "string"},
                {"description", "The skill id to load."}}}}},
            {"required", {"skill_id"}}};
}

QString LoadSkillTool::execute(const json &args)
{
    if (!m_manager)
        return QStringLiteral("Error: skill manager is unavailable.");
    const QString skillId =
        QString::fromStdString(args.value("skill_id", std::string())).trimmed();
    if (skillId.isEmpty())
        return QStringLiteral("Error: load_skill requires a non-empty `skill_id`.");
    return m_manager->loadSkill(skillId);
}
