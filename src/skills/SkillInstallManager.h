#pragma once

#include <QAbstractListModel>
#include <QVector>

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

class SkillInstallManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY skillsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

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

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool installSkillPackage(const QString &archivePath);
    Q_INVOKABLE bool uninstallSkill(const QString &skillId);
    Q_INVOKABLE bool setSkillEnabled(const QString &skillId, bool enabled);

  signals:
    void skillsChanged();
    void lastErrorChanged();
    void skillInstalled(const QString &skillId);
    void skillInstallFailed(const QString &error);

  private:
    void setError(const QString &error);
    int indexOf(const QString &skillId) const;

    Config *m_config = nullptr;
    Settings *m_settings = nullptr;
    SkillManager *m_skillManager = nullptr;
    QVector<InstalledSkillInfo> m_skills;
    QString m_lastError;
};