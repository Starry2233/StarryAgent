#include "ReadSkillReferenceTool.h"

#include "skills/SkillManager.h"

using json = nlohmann::json;

ReadSkillReferenceTool::ReadSkillReferenceTool(SkillManager *manager)
    : m_manager(manager)
{
}

QString ReadSkillReferenceTool::description() const
{
    return QStringLiteral("Read a file inside an installed skill pack, such as a document under references/.");
}

json ReadSkillReferenceTool::schema() const
{
    return {{"type", "object"},
            {"properties",
             {{"skill_id",
               {{"type", "string"},
                {"description", "The skill id."}}},
              {"path",
               {{"type", "string"},
                {"description",
                 "Relative path inside the skill, such as references/api.md."}}}}},
            {"required", {"skill_id", "path"}}};
}

QString ReadSkillReferenceTool::execute(const json &args)
{
    if (!m_manager)
        return QStringLiteral("Error: skill manager is unavailable.");
    const QString skillId =
        QString::fromStdString(args.value("skill_id", std::string())).trimmed();
    const QString path =
        QString::fromStdString(args.value("path", std::string())).trimmed();
    if (skillId.isEmpty())
    {
        return QStringLiteral(
            "Error: read_skill_reference requires a non-empty `skill_id`.");
    }
    if (path.isEmpty())
    {
        return QStringLiteral(
            "Error: read_skill_reference requires a non-empty `path`.");
    }
    return m_manager->readReference(skillId, path);
}
