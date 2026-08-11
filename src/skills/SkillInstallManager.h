#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "SkillPackageLoader.h"
#include "backend/StarryAgentBackendGlobal.h"

class Config;
class Settings;
class SkillManager;

struct InstalledSkillInfo
{
    QString skillId;
    QString name;
    QString description;
    QString path;
    int referenceCount = 0;
    bool enabled = true;
};

struct PendingChildSkillInfo
{
    QString skillId;
    QString name;
    QString description;
    QString relativePath;
    bool installed = false;
};

class STARRYAGENT_BACKEND_EXPORT SkillInstallManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY skillsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool hasPendingChildSelection READ hasPendingChildSelection NOTIFY pendingChildSelectionChanged)
    Q_PROPERTY(QString pendingParentSkillId READ pendingParentSkillId NOTIFY pendingChildSelectionChanged)
    Q_PROPERTY(QString pendingParentSkillName READ pendingParentSkillName NOTIFY pendingChildSelectionChanged)
    Q_PROPERTY(QVariantList pendingChildSkills READ pendingChildSkills NOTIFY pendingChildSelectionChanged)

  public:
    enum Role
    {
        SkillIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        PathRole,
        ReferenceCountRole,
        EnabledRole
    };

    explicit SkillInstallManager(Config *config, Settings *settings,
                                 SkillManager *skillManager,
                                 QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString lastError() const { return m_lastError; }
    bool hasPendingChildSelection() const { return !m_pendingArchivePath.isEmpty() && !m_pendingChildren.isEmpty(); }
    QString pendingParentSkillId() const { return m_pendingParent.id; }
    QString pendingParentSkillName() const { return m_pendingParent.name; }
    QVariantList pendingChildSkills() const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool installSkillPackage(const QString &archivePath);
    Q_INVOKABLE bool completePendingChildInstall(const QString &relativeSkillPath);
    Q_INVOKABLE void clearPendingChildSelection();
    Q_INVOKABLE bool uninstallSkill(const QString &skillId);
    Q_INVOKABLE bool setSkillEnabled(const QString &skillId, bool enabled);

  signals:
    void skillsChanged();
    void lastErrorChanged();
    void skillInstalled(const QString &skillId);
    void skillInstallFailed(const QString &error);
    void pendingChildSelectionChanged();

  private:
    void setError(const QString &error);
    void setPendingSelection(const QString &archivePath,
                             const SkillPackageMetadata &parent,
                             const QList<PendingChildSkillInfo> &children);
    void clearPendingSelectionState();
    int indexOf(const QString &skillId) const;

    Config *m_config = nullptr;
    Settings *m_settings = nullptr;
    SkillManager *m_skillManager = nullptr;
    QVector<InstalledSkillInfo> m_skills;
    QString m_lastError;
    QString m_pendingArchivePath;
    SkillPackageMetadata m_pendingParent;
    QList<PendingChildSkillInfo> m_pendingChildren;
};