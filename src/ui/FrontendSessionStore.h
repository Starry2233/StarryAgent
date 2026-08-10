#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "chat/Conversation.h"
#include "chat/ConversationManager.h"

class FrontendSessionStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(ConversationManager *conversationManager READ conversationManager WRITE setConversationManager NOTIFY conversationManagerChanged)
    Q_PROPERTY(Conversation *activeConversation READ activeConversation NOTIFY activeConversationChanged)
    Q_PROPERTY(int count READ count NOTIFY sessionsChanged)

  public:
    enum Roles
    {
        ConversationRole = Qt::UserRole + 1,
        IdRole,
        TitleRole,
        ModeIdRole,
        CreatedRole,
        UpdatedRole,
        BucketRole,
        RelativeTimeRole,
        ActiveRole,
    };
    Q_ENUM(Roles)

    explicit FrontendSessionStore(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    ConversationManager *conversationManager() const { return m_conversationManager; }
    void setConversationManager(ConversationManager *manager);

    Conversation *activeConversation() const { return m_activeConversation; }
    int count() const;

    Q_INVOKABLE Conversation *conversationAt(int index) const;
    Q_INVOKABLE Conversation *conversationById(const QString &id) const;
    Q_INVOKABLE void setActiveConversation(Conversation *conversation);
    Q_INVOKABLE void openNewConversation(const QString &modeId);
    Q_INVOKABLE void removeConversation(Conversation *conversation);
    Q_INVOKABLE void renameConversation(Conversation *conversation, const QString &newTitle);
    Q_INVOKABLE void saveActiveConversation();
    Q_INVOKABLE void sendWithImages(const QString &text, const QStringList &imagePaths);
    Q_INVOKABLE void appendAssistantText(const QString &text);
    Q_INVOKABLE void appendUser(const QString &text);
    Q_INVOKABLE QString compactNow(const QString &instructions = QString());
    Q_INVOKABLE QString enterPlanMode(const QString &note = QString());
    Q_INVOKABLE QString submitPlanForApproval();
    Q_INVOKABLE QString approvePlan();
    Q_INVOKABLE QString rejectPlan(const QString &note = QString());
    Q_INVOKABLE QString exitPlanMode();
    Q_INVOKABLE QString showPlan() const;
    Q_INVOKABLE QString clearPlan();
    Q_INVOKABLE QString openPlan();
    Q_INVOKABLE QString showWorkdir() const;
    Q_INVOKABLE QString setWorkdir(const QString &path);
    Q_INVOKABLE QString resetWorkdir();
    Q_INVOKABLE QString importImage(const QString &sourcePath);
    Q_INVOKABLE void dispatch(const QString &toolCallId, const QString &name,
                              const QString &argsText);
    Q_INVOKABLE void denyTool(const QString &toolCallId);

  signals:
    void conversationManagerChanged();
    void activeConversationChanged();
    void sessionsChanged();

  private slots:
    void syncFromManager();

  private:
    struct Row
    {
        Conversation *conversation = nullptr;
        QString id;
        QString title;
        QString modeId;
        QDateTime created;
        QDateTime updated;
        QString bucket;
        QString relativeTime;
        bool active = false;
    };

    void rebuildRows();
    void refreshActiveConversation();
    QString bucketOf(const QDateTime &updated) const;
    QString relativeTimeOf(const QDateTime &updated) const;
    int rowIndexForConversation(Conversation *conversation) const;

    ConversationManager *m_conversationManager = nullptr;
    Conversation *m_activeConversation = nullptr;
    QList<Row> m_rows;
};
