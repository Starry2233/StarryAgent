#include "SkillInstallManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "SkillManager.h"
#include "SkillPackageLoader.h"
#include "core/Config.h"
#include "core/Settings.h"

SkillInstallManager::SkillInstallManager(Config *config, Settings *settings,
                                         SkillManager *skillManager,
                                         QObject *parent)
    : QAbstractListModel(parent), m_config(config), m_settings(settings),
      m_skillManager(skillManager)
{
    reload();
    if (m_config)
        connect(m_config, &Config::rootDirChanged, this, [this] {
            clearPendingSelectionState();
            reload();
        });
}

int SkillInstallManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_skills.size();
}

QVariant SkillInstallManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_skills.size())
        return {};
    const InstalledSkillInfo &skill = m_skills.at(index.row());
    switch (role)
    {
    case SkillIdRole:
        return skill.skillId;
    case NameRole:
        return skill.name;
    case DescriptionRole:
        return skill.description;
    case PathRole:
        return skill.path;
    case ReferenceCountRole:
        return skill.referenceCount;
    case EnabledRole:
        return skill.enabled;
    default:
        return {};
    }
}

QHash<int, QByteArray> SkillInstallManager::roleNames() const
{
    return {{SkillIdRole, "skillId"},
            {NameRole, "name"},
            {DescriptionRole, "description"},
            {PathRole, "path"},
            {ReferenceCountRole, "referenceCount"},
            {EnabledRole, "enabled"}};
}

QVariantList SkillInstallManager::pendingChildSkills() const
{
    QVariantList items;
    items.reserve(m_pendingChildren.size());
    for (const PendingChildSkillInfo &child : m_pendingChildren)
    {
        QVariantMap entry;
        entry.insert(QStringLiteral("skillId"), child.skillId);
        entry.insert(QStringLiteral("name"), child.name);
        entry.insert(QStringLiteral("description"), child.description);
        entry.insert(QStringLiteral("relativePath"), child.relativePath);
        entry.insert(QStringLiteral("installed"), child.installed);
        items.append(entry);
    }
    return items;
}

void SkillInstallManager::reload()
{
    beginResetModel();
    m_skills.clear();

    if (m_config && !m_config->skillsPath().isEmpty())
    {
        QDir root(m_config->skillsPath());
        root.mkpath(QStringLiteral("."));
        const QFileInfoList dirs =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &dir : dirs)
        {
            const QString skillFile =
                QDir(dir.absoluteFilePath()).filePath(QStringLiteral("SKILL.md"));
            if (!QFileInfo::exists(skillFile))
                continue;

            InstalledSkillInfo info;
            info.skillId = dir.fileName();
            info.name = dir.fileName();
            info.path = dir.absoluteFilePath();
            info.enabled = !m_settings || m_settings->isSkillEnabled(info.skillId);
            if (m_skillManager)
                info.referenceCount =
                    m_skillManager->listReferences(info.skillId).size();

            QFile file(skillFile);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                const QString content = QString::fromUtf8(file.readAll());
                const QStringList lines = content.split(QLatin1Char('\n'));
                bool inFrontmatter = false;
                bool frontmatterParsed = false;
                for (const QString &line : lines)
                {
                    const QString trimmed = line.trimmed();
                    if (!frontmatterParsed && trimmed == QStringLiteral("---"))
                    {
                        inFrontmatter = !inFrontmatter;
                        frontmatterParsed = !inFrontmatter;
                        continue;
                    }
                    if (!inFrontmatter)
                        continue;
                    const int colon = trimmed.indexOf(QLatin1Char(':'));
                    if (colon <= 0)
                        continue;
                    const QString key = trimmed.left(colon).trimmed();
                    QString value = trimmed.mid(colon + 1).trimmed();
                    if (value.size() >= 2 &&
                        ((value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\'')) ||
                         (value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))))
                    {
                        value = value.mid(1, value.size() - 2).trimmed();
                    }
                    if (key == QStringLiteral("name") && !value.isEmpty())
                        info.name = value;
                    else if (key == QStringLiteral("description"))
                        info.description = value;
                }
            }
            m_skills.append(info);
        }
    }

    endResetModel();
    emit skillsChanged();
}

bool SkillInstallManager::installSkillPackage(const QString &archivePath)
{
    if (!m_config || m_config->skillsPath().isEmpty())
    {
        setError(QStringLiteral("Skill storage is not ready."));
        emit skillInstallFailed(m_lastError);
        return false;
    }

    SkillPackageInspection inspection;
    QString error;
    if (!SkillPackageLoader::inspectSkillPackage(archivePath, &inspection, &error))
    {
        setError(error);
        emit skillInstallFailed(error);
        return false;
    }

    SkillPackageMetadata parentMetadata;
    if (!SkillPackageLoader::installFromPackage(archivePath, m_config->skillsPath(),
                                                inspection.parent.relativePath,
                                                &parentMetadata, &error))
    {
        setError(error);
        emit skillInstallFailed(error);
        return false;
    }

    if (m_skillManager)
        m_skillManager->rescan();
    reload();

    QList<PendingChildSkillInfo> children;
    children.reserve(inspection.children.size());
    for (const SkillPackageMetadata &child : inspection.children)
    {
        PendingChildSkillInfo item;
        item.skillId = child.id;
        item.name = child.name;
        item.description = child.description;
        item.relativePath = child.relativePath;
        item.installed = indexOf(child.id) >= 0;
        children.append(item);
    }

    if (!children.isEmpty())
        setPendingSelection(archivePath, parentMetadata, children);
    else
        clearPendingSelectionState();

    setError(QString());
    emit skillInstalled(parentMetadata.id);
    return true;
}

bool SkillInstallManager::completePendingChildInstall(const QString &relativeSkillPath)
{
    if (!hasPendingChildSelection())
        return false;

    const QString selectedPath = relativeSkillPath.trimmed();
    if (selectedPath.isEmpty())
        return false;

    for (PendingChildSkillInfo &child : m_pendingChildren)
    {
        if (child.relativePath != selectedPath)
            continue;
        if (child.installed)
        {
            setError(QString());
            emit pendingChildSelectionChanged();
            return true;
        }

        QString error;
        SkillPackageMetadata childMetadata;
        if (!SkillPackageLoader::installFromPackage(m_pendingArchivePath,
                                                    m_config ? m_config->skillsPath() : QString(),
                                                    selectedPath, &childMetadata, &error))
        {
            setError(error);
            emit skillInstallFailed(error);
            return false;
        }

        child.installed = true;
        if (m_skillManager)
            m_skillManager->rescan();
        reload();
        setError(QString());
        emit pendingChildSelectionChanged();
        emit skillInstalled(childMetadata.id);
        return true;
    }

    return false;
}

void SkillInstallManager::clearPendingChildSelection()
{
    clearPendingSelectionState();
}

bool SkillInstallManager::uninstallSkill(const QString &skillId)
{
    const QString id = skillId.trimmed();
    const int idx = indexOf(id);
    if (idx < 0)
    {
        setError(QStringLiteral("Skill is not installed: %1").arg(id));
        return false;
    }

    const QString path = m_skills.at(idx).path;
    if (!QDir(path).removeRecursively())
    {
        setError(QStringLiteral("Failed to remove skill: %1").arg(id));
        return false;
    }

    if (m_settings)
        m_settings->clearSkillState(id);
    if (m_skillManager)
        m_skillManager->rescan();
    reload();
    setError(QString());
    return true;
}

bool SkillInstallManager::setSkillEnabled(const QString &skillId, bool enabled)
{
    const QString id = skillId.trimmed();
    if (id.isEmpty())
        return false;

    if (m_settings)
        m_settings->setSkillEnabled(id, enabled);

    const int idx = indexOf(id);
    if (idx >= 0)
    {
        m_skills[idx].enabled = m_settings ? m_settings->isSkillEnabled(id) : enabled;
        emit dataChanged(index(idx), index(idx), {EnabledRole});
    }

    if (m_skillManager)
        m_skillManager->rescan();
    return true;
}

void SkillInstallManager::setError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void SkillInstallManager::setPendingSelection(
    const QString &archivePath, const SkillPackageMetadata &parent,
    const QList<PendingChildSkillInfo> &children)
{
    m_pendingArchivePath = archivePath;
    m_pendingParent = parent;
    m_pendingChildren = children;
    emit pendingChildSelectionChanged();
}

void SkillInstallManager::clearPendingSelectionState()
{
    if (m_pendingArchivePath.isEmpty() && m_pendingChildren.isEmpty() &&
        m_pendingParent.id.isEmpty())
    {
        return;
    }
    m_pendingArchivePath.clear();
    m_pendingParent = SkillPackageMetadata();
    m_pendingChildren.clear();
    emit pendingChildSelectionChanged();
}

int SkillInstallManager::indexOf(const QString &skillId) const
{
    for (int i = 0; i < m_skills.size(); ++i)
    {
        if (m_skills.at(i).skillId == skillId)
            return i;
    }
    return -1;
}
