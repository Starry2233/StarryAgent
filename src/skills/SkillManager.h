#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <utility>

struct SkillInfo
{
    QString id;
    QString description;
    QString dirPath;
    QString skillMdPath;
    QString rawFrontmatter;
    QString body;
};

class Settings;

class SkillManager
{
  public:
    explicit SkillManager(QString skillsDir, Settings *settings = nullptr);

    QString buildSkillIndexPrompt();
    QString loadSkill(const QString &skillId);
    QString readReference(const QString &skillId, const QString &relativePath);
    QStringList listReferences(const QString &skillId);
    void rescan();

  private:
    void scanSkills();
    const SkillInfo *findSkill(const QString &skillId) const;
    bool isSkillEnabled(const SkillInfo &skill) const;
    void clearCachedState();

    QString m_skillsDir;
    Settings *m_settings = nullptr;
    QList<SkillInfo> m_skills;
    QString m_cachedIndex;
    bool m_scanned = false;
};
