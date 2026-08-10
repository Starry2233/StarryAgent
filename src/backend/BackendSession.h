#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

class Conversation;
class ConversationManager;
class Settings;
class QString;
class QJsonObject;

namespace StarryAgent
{
class BackendSession : public QObject
{
    Q_OBJECT

  public:
    explicit BackendSession(ConversationManager *conversationManager,
                            Settings *settings, QObject *parent = nullptr);

    QJsonObject bootstrap() const;
    QJsonObject handleRequest(const QJsonObject &request);

  signals:
    void eventReady(const QJsonObject &event);

  private:
    void observeConversation(Conversation *conversation);
    void emitConversationsReset();
    void emitActiveConversationChanged();
    void emitConversationRowsInserted(Conversation *conversation, int first,
                                      int last);
    void emitConversationRowsUpdated(Conversation *conversation, int first,
                                     int last);
    void emitConversationRowsRemoved(Conversation *conversation, int first,
                                     int last);
    void emitConversationStreamingChanged(Conversation *conversation);
    void emitConversationToolRowsChanged(Conversation *conversation,
                                         const QString &eventName);
    void emitConversationErrorChanged(Conversation *conversation);
    void emitConversationPlanChanged(Conversation *conversation);
    void emitConversationWorkdirChanged(Conversation *conversation);
    void emitSettingsChanged();
    Conversation *requireConversation(const QString &conversationId,
                                      QJsonObject *errorOut) const;

    ConversationManager *m_conversationManager = nullptr;
    Settings *m_settings = nullptr;
};
} // namespace StarryAgent
