#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonObject>
#include <QTimer>

class Config;
class Settings;
class ConversationManager;

#include "backend/StarryAgentBackendGlobal.h"

// Persistent, main-thread scheduler. Tasks are stored independently from
// conversations so global tasks remain meaningful after their origin is gone.
class STARRYAGENT_BACKEND_EXPORT ScheduledTaskManager final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY tasksChanged)
    Q_PROPERTY(bool globalEnabled READ globalEnabled WRITE setGlobalEnabled
                   NOTIFY globalEnabledChanged)

  public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        PromptRole,
        ScopeRole,
        ConversationIdRole,
        ConversationTitleRole,
        NextRunRole,
        IntervalMinutesRole,
        RecurrenceRole,
        TempRole,
        EnabledRole,
        StatusRole,
        LastRunRole,
        LastResultRole
    };
    struct Task
    {
        QString id;
        QString prompt;
        QString scope; // conversation | global
        QString conversationId;
        QString conversationTitle;
        QDateTime nextRun;
        int intervalMinutes = 0;
        QString recurrence = "once"; // once | interval | daily | weekly | monthly
        bool temp = false;
        bool enabled = true;
        QString status;
        QDateTime lastRun;
        QString lastResult;
    };

    ScheduledTaskManager(Config *config, Settings *settings,
                         ConversationManager *conversations,
                         QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool globalEnabled() const;
    void setGlobalEnabled(bool enabled);

    Q_INVOKABLE void setTaskEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void deleteTask(const QString &id);
    Q_INVOKABLE QString taskSummary() const;

    QString createFromTool(const QString &conversationId, const QString &prompt,
                           const QString &scope, const QString &runAt,
                           int intervalMinutes, const QString &recurrence,
                           bool temp,
                           const QString &requestedId = {});
    QString searchFromTool(const QString &conversationId, const QString &query,
                           const QString &scope, int limit) const;
    QString deleteFromTool(const QString &conversationId,
                           const QString &idOrQuery);
    void removeConversationTasks(const QString &conversationId);
    void completeRun(const QString &taskId, bool success,
                     const QString &result);

  signals:
    void tasksChanged();
    void globalEnabledChanged();
    void taskNotification(const QString &title, const QString &message);

  private slots:
    void checkDueTasks();

  private:
    void load();
    void save() const;
    int indexOf(const QString &id) const;
    bool isGlobalAllowed(const Task &task) const;
    void trigger(Task &task);
    QString resolveTaskId(const QString &idOrQuery,
                          const QString &conversationId) const;
    QString formattedTask(const Task &task) const;
    static QString normalizeRecurrence(const QString &recurrence,
                                       int intervalMinutes);
    static QDateTime computeNextRun(const Task &task, const QDateTime &baseTime);
    static QJsonObject toJson(const Task &task);
    static Task fromJson(const QJsonObject &json);

    Config *m_config = nullptr;
    Settings *m_settings = nullptr;
    ConversationManager *m_conversations = nullptr;
    QVector<Task> m_tasks;
    QTimer m_timer;
};
