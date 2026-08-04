#include "SkillManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <utility>

#include "core/Settings.h"

namespace
{
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream stream(&file);
    return stream.readAll();
}

QString normalizeNewlines(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString stripQuotes(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2)
    {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\'') && last == QLatin1Char('\'')) ||
            (first == QLatin1Char('"') && last == QLatin1Char('"')))
        {
            value = value.mid(1, value.size() - 2);
        }
    }
    return value.trimmed();
}

SkillInfo parseSkillFile(const QString &skillMdPath)
{
    SkillInfo info;
    QFileInfo fileInfo(skillMdPath);
    info.dirPath = fileInfo.dir().absolutePath();
    info.skillMdPath = skillMdPath;
    info.id = fileInfo.dir().dirName();

    const QString raw = readTextFile(skillMdPath);
    if (raw.isEmpty())
        return info;

    const QString normalized = normalizeNewlines(raw);
    if (!normalized.startsWith(QStringLiteral("---\n")))
    {
        info.body = normalized.trimmed();
        return info;
    }

    const QStringList lines = normalized.split(QLatin1Char('\n'));
    int closingFence = -1;
    for (int i = 1; i < lines.size(); ++i)
    {
        if (lines.at(i).trimmed() == QStringLiteral("---"))
        {
            closingFence = i;
            break;
        }
    }
    if (closingFence <= 0)
    {
        info.body = normalized.trimmed();
        return info;
    }

    const QStringList frontmatterLines = lines.mid(1, closingFence - 1);
    info.rawFrontmatter = frontmatterLines.join(QLatin1Char('\n')).trimmed();
    info.body = lines.mid(closingFence + 1).join(QLatin1Char('\n')).trimmed();

    for (int i = 0; i < frontmatterLines.size(); ++i)
    {
        const QString line = frontmatterLines.at(i);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;
        const int colon = trimmed.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;

        const QString key = trimmed.left(colon).trimmed();
        QString value = trimmed.mid(colon + 1).trimmed();
        if (key == QStringLiteral("name") && !value.isEmpty())
            info.id = stripQuotes(value);
        else if (key == QStringLiteral("description"))
            info.description = stripQuotes(value);
    }

    return info;
}

QStringList gatherReferenceFiles(const QString &skillDir)
{
    QStringList references;
    QDirIterator it(skillDir, QDir::Files, QDirIterator::Subdirectories);
    const QDir base(skillDir);
    while (it.hasNext())
    {
        const QString absolutePath = it.next();
        const QString relativePath = QDir::fromNativeSeparators(
            base.relativeFilePath(absolutePath));
        if (relativePath.compare(QStringLiteral("SKILL.md"), Qt::CaseInsensitive) == 0)
            continue;
        references.append(relativePath);
    }
    std::sort(references.begin(), references.end(),
              [](const QString &a, const QString &b)
              { return a.compare(b, Qt::CaseInsensitive) < 0; });
    return references;
}

} // namespace

SkillManager::SkillManager(QString skillsDir, Settings *settings)
    : m_skillsDir(std::move(skillsDir)), m_settings(settings)
{
}

QString SkillManager::buildSkillIndexPrompt()
{
    scanSkills();
    if (!m_cachedIndex.isEmpty())
        return m_cachedIndex;

    QStringList lines;
    lines.append(QStringLiteral("Available skills:"));
    for (const SkillInfo &skill : m_skills)
    {
        if (!isSkillEnabled(skill))
            continue;
        QString line = QStringLiteral("- %1").arg(skill.id);
        if (!skill.description.isEmpty())
            line += QStringLiteral(" — %1").arg(skill.description);
        lines.append(line);
    }
    if (lines.size() == 1)
        return {};
    lines.append(QString());
    lines.append(QStringLiteral("Call `load_skill` with a skill id to get full instructions. Call `read_skill_reference` to read auxiliary files inside a skill."));
    m_cachedIndex = lines.join(QLatin1Char('\n')).trimmed();
    return m_cachedIndex;
}

QString SkillManager::loadSkill(const QString &skillId)
{
    scanSkills();
    const SkillInfo *skill = findSkill(skillId);
    if (!skill || !isSkillEnabled(*skill))
        return QStringLiteral("Error: skill '%1' not found.").arg(skillId);

    QStringList lines;
    lines.append(QStringLiteral("Skill: %1").arg(skill->id));
    if (!skill->description.isEmpty())
        lines.append(QStringLiteral("Description: %1").arg(skill->description));
    lines.append(QString());
    lines.append(QStringLiteral("---"));
    if (!skill->rawFrontmatter.isEmpty())
    {
        lines.append(QStringLiteral("---"));
        lines.append(skill->rawFrontmatter);
        lines.append(QStringLiteral("---"));
    }
    if (!skill->body.isEmpty())
        lines.append(skill->body);
    lines.append(QStringLiteral("---"));

    const QStringList references = gatherReferenceFiles(skill->dirPath);
    if (references.isEmpty())
    {
        lines.append(QString());
        lines.append(QStringLiteral("Reference files available: none"));
    }
    else
    {
        lines.append(QString());
        lines.append(QStringLiteral("Reference files available:"));
        for (const QString &path : references)
            lines.append(QStringLiteral("- %1").arg(path));
        lines.append(QString());
        lines.append(QStringLiteral("Use read_skill_reference to read any of these."));
    }
    return lines.join(QLatin1Char('\n')).trimmed();
}

QString SkillManager::readReference(const QString &skillId,
                                    const QString &relativePath)
{
    scanSkills();
    const SkillInfo *skill = findSkill(skillId);
    if (!skill || !isSkillEnabled(*skill))
        return QStringLiteral("Error: skill '%1' not found.").arg(skillId);

    const QString trimmedPath = relativePath.trimmed();
    if (trimmedPath.isEmpty())
        return QStringLiteral("Error: path must not be empty.");
    if (QDir::isAbsolutePath(trimmedPath) || trimmedPath.startsWith(QStringLiteral("../")) ||
        trimmedPath.contains(QStringLiteral("/../")) || trimmedPath.contains(QStringLiteral("..\\")) ||
        trimmedPath.contains(QStringLiteral("\\..\\")))
    {
        return QStringLiteral("Error: path must stay within the skill directory.");
    }

    const QString canonicalSkillDir = QFileInfo(skill->dirPath).canonicalFilePath();
    if (canonicalSkillDir.isEmpty())
        return QStringLiteral("Error: skill directory is unavailable.");

    const QString candidatePath = QDir(skill->dirPath).filePath(trimmedPath);
    const QString canonicalCandidate = QFileInfo(candidatePath).canonicalFilePath();
    if (canonicalCandidate.isEmpty())
        return QStringLiteral("Error: reference '%1' not found.").arg(trimmedPath);
    if (!canonicalCandidate.startsWith(canonicalSkillDir + QDir::separator()) &&
        canonicalCandidate != canonicalSkillDir)
    {
        return QStringLiteral("Error: path must stay within the skill directory.");
    }

    const QString content = readTextFile(canonicalCandidate);
    if (content.isEmpty() && !QFileInfo::exists(canonicalCandidate))
        return QStringLiteral("Error: reference '%1' not found.").arg(trimmedPath);

    return QStringLiteral("Skill: %1\nPath: %2\n\n%3")
        .arg(skill->id, trimmedPath, normalizeNewlines(content).trimmed());
}

QStringList SkillManager::listReferences(const QString &skillId)
{
    scanSkills();
    const SkillInfo *skill = findSkill(skillId);
    return skill && isSkillEnabled(*skill) ? gatherReferenceFiles(skill->dirPath)
                                           : QStringList();
}

void SkillManager::rescan()
{
    clearCachedState();
    scanSkills();
}

void SkillManager::scanSkills()
{
    if (m_scanned)
        return;
    m_scanned = true;
    m_skills.clear();
    m_cachedIndex.clear();

    if (m_settings)
    {
        QObject::connect(m_settings, &Settings::disabledSkillIdsChanged,
                         [this] { clearCachedState(); });
    }

    if (m_skillsDir.trimmed().isEmpty())
        return;

    QDirIterator it(m_skillsDir, {QStringLiteral("SKILL.md")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const SkillInfo info = parseSkillFile(it.next());
        if (info.id.trimmed().isEmpty())
            continue;
        m_skills.append(info);
    }

    std::sort(m_skills.begin(), m_skills.end(),
              [](const SkillInfo &a, const SkillInfo &b)
              { return a.id.toLower() < b.id.toLower(); });
}

const SkillInfo *SkillManager::findSkill(const QString &skillId) const
{
    const QString needle = skillId.trimmed();
    for (const SkillInfo &skill : m_skills)
    {
        if (skill.id.compare(needle, Qt::CaseInsensitive) == 0)
            return &skill;
    }
    return nullptr;
}

bool SkillManager::isSkillEnabled(const SkillInfo &skill) const
{
    return !m_settings || m_settings->isSkillEnabled(skill.id);
}

void SkillManager::clearCachedState()
{
    m_scanned = false;
    m_cachedIndex.clear();
    m_skills.clear();
}
