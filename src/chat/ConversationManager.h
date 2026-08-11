#pragma once

#include <QList>
#include <QObject>
#include <QQmlListProperty>
#include <QString>

#include "Conversation.h"
#include "backend/StarryAgentBackendGlobal.h"

class Config;
class Settings;
class ToolRegistry;
class ScheduledTaskManager;

// ConversationManager — owns all conversations, exposes them to QML (sidebar),
// tracks the active one (ChatView), and persists each to
// <root>/conversations/<id>.json.
class STARRYAGENT_BACKEND_EXPORT ConversationManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<Conversation> conversations READ conversations
                   NOTIFY conversationsChanged)
    Q_PROPERTY(
        Conversation *active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY conversationsChanged)

  public:
    explicit ConversationManager(Config *config, Settings *settings,
                                 ToolRegistry *registry,
                                 bool loadHistory = true,
                                 QObject *parent = nullptr);
    ~ConversationManager() override;

    QQmlListProperty<Conversation> conversations();
    QList<Conversation *> list() const { return m_list; }
    int count() const { return m_list.size(); }
    Conversation *active() const { return m_active; }
    Conversation *findById(const QString &id) const;
    void setActive(Conversation *c);
    void setScheduledTaskManager(ScheduledTaskManager *manager);

    // Create a new conversation in the given mode; becomes active.
    Q_INVOKABLE Conversation *newConversation(const QString &modeId);
    Q_INVOKABLE void remove(Conversation *c);
    Q_INVOKABLE void rename(Conversation *c, const QString &newTitle);
    Q_INVOKABLE void saveActive(); // persist active to disk

  signals:
    void conversationsChanged();
    void activeChanged();

  private:
    void loadAll();
    void observeConversation(Conversation *c);
    void save(Conversation *c) const;
    QString conversationsDir() const;
    QString plansDir() const;

    Config *m_config;
    Settings *m_settings;
    ToolRegistry *m_registry;
    QString m_indexMd;
    QString m_skillsMd;
    QList<Conversation *> m_list;
    Conversation *m_active = nullptr;
    ScheduledTaskManager *m_scheduledTasks = nullptr;
};
