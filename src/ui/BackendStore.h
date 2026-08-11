#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QJsonObject>
#include <QVariant>
#include <QVariantMap>
#include <QString>
#include <QStringList>
#include <QVector>

class QJsonArray;
class Settings;

namespace StarryAgent
{
class IBackendTransport;

class BackendStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *activeConversationModel READ
                   activeConversationModel NOTIFY activeConversationChanged)
    Q_PROPERTY(QVariant activeConversation READ activeConversation NOTIFY
                   activeConversationChanged)
    Q_PROPERTY(QString activeConversationId READ activeConversationId NOTIFY
                   activeConversationChanged)
    Q_PROPERTY(bool activeConversationStreaming READ activeConversationStreaming
                   NOTIFY activeConversationChanged)
    Q_PROPERTY(QString activeConversationError READ activeConversationError NOTIFY
                   activeConversationChanged)
    Q_PROPERTY(bool activeConversationPlanMode READ activeConversationPlanMode NOTIFY
                   activeConversationChanged)
    Q_PROPERTY(QString activeConversationPlanText READ activeConversationPlanText NOTIFY
                   activeConversationChanged)
    Q_PROPERTY(bool activeConversationPlanAwaitingApproval READ
                   activeConversationPlanAwaitingApproval NOTIFY
                       activeConversationChanged)
    Q_PROPERTY(QString activeConversationWorkdir READ activeConversationWorkdir NOTIFY
                   activeConversationChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

  public:
    enum ConversationRoles
    {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ModeIdRole,
        CreatedRole,
        UpdatedRole,
        StreamingRole,
        ErrorRole,
        PlanModeRole,
        PlanTextRole,
        PlanAwaitingApprovalRole,
        WorkdirRole,
        ActiveRole,
        BucketRole,
        RelativeTimeRole,
    };

    enum MessageRoles
    {
        KindRole = Qt::UserRole + 1,
        TextRole,
        ToolCallIdRole,
        ToolNameRole,
        ArgsTextRole,
        StatusRole,
        ResultRole,
        NeedsApprovalRole,
        ImagePathsRole,
    };

    explicit BackendStore(QObject *parent = nullptr);

    void setSettings(::Settings *settings);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QAbstractItemModel *activeConversationModel() const;
    QVariant activeConversation() const;
    QString activeConversationId() const { return m_activeConversationId; }
    bool activeConversationStreaming() const;
    QString activeConversationError() const;
    bool activeConversationPlanMode() const;
    QString activeConversationPlanText() const;
    bool activeConversationPlanAwaitingApproval() const;
    QString activeConversationWorkdir() const;
    bool connected() const;
    int count() const { return m_conversations.size(); }

    void setTransport(IBackendTransport *transport);
    Q_INVOKABLE bool requestBootstrap();
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void saveActiveConversation();
    Q_INVOKABLE void appendUser(const QString &text);

    Q_INVOKABLE void openNewConversation(const QString &modeId);
    Q_INVOKABLE void removeConversationById(const QString &conversationId);
    Q_INVOKABLE void renameConversationById(const QString &conversationId,
                                            const QString &title);
    Q_INVOKABLE void setActiveConversationById(const QString &conversationId);
    Q_INVOKABLE void sendWithImages(const QString &text,
                                    const QStringList &imagePaths);
    Q_INVOKABLE QString compactNow(const QString &instructions = QString());
    Q_INVOKABLE QString enterPlanMode(const QString &note = QString());
    Q_INVOKABLE QString submitPlanForApproval();
    Q_INVOKABLE QString approvePlan();
    Q_INVOKABLE QString rejectPlan(const QString &note);
    Q_INVOKABLE QString exitPlanMode();
    Q_INVOKABLE QString showPlan() const;
    Q_INVOKABLE QString clearPlan();
    Q_INVOKABLE QString openPlan();
    Q_INVOKABLE QString showWorkdir() const;
    Q_INVOKABLE QString setWorkdir(const QString &path);
    Q_INVOKABLE QString resetWorkdir();
    Q_INVOKABLE void dispatch(const QString &toolCallId, const QString &name,
                              const QString &argsText);
    Q_INVOKABLE void denyTool(const QString &toolCallId);
    Q_INVOKABLE QString importImage(const QString &sourcePath);
    Q_INVOKABLE void appendAssistantText(const QString &text);
    Q_INVOKABLE void syncSettings();

  signals:
    void activeConversationChanged();
    void sessionsChanged();
    void connectedChanged();
    void countChanged();
    void commandCompleted(const QString &requestId, const QString &method,
                          const QJsonObject &result, const QJsonObject &error);

  private slots:
    void onTransportMessage(const QJsonObject &message);
    void onTransportStopped();

  private:
    struct ConversationSummary
    {
        QString id;
        QString title;
        QString modeId;
        QString created;
        QString updated;
        bool streaming = false;
        QString error;
        bool planMode = false;
        QString planText;
        bool planAwaitingApproval = false;
        QString workdir;
        bool active = false;
    };

    struct MessageRow
    {
        QString kind;
        QString text;
        QString toolCallId;
        QString toolName;
        QString argsText;
        QString status;
        QString result;
        bool needsApproval = false;
        QStringList imagePaths;
    };

    class MessageModel;

    void resetState();
    QString nextRequestId();
    bool sendRequest(const QString &method, const QJsonObject &params = {},
                     QString *requestIdOut = nullptr);
    QString sendTextCommand(const QString &method,
                            const QJsonObject &params = {});
    QJsonObject sendSyncRequest(const QString &method,
                                const QJsonObject &params = {});
    void applyBootstrap(const QJsonObject &payload);
    void applyConversations(const QJsonArray &items);
    void applyActiveRows(const QJsonArray &rows);
    void applySettingsSnapshot(const QJsonObject &snapshot);
    void updateActiveConversationSummary(const QJsonObject &patch);
    int conversationIndexById(const QString &conversationId) const;
    ConversationSummary conversationFromJson(const QJsonObject &obj) const;
    MessageRow messageFromJson(const QJsonObject &obj) const;
    void upsertConversation(const ConversationSummary &summary);
    void emitConversationDataChanged(int row);
    void refreshActiveConversationData();

    IBackendTransport *m_transport = nullptr;
    ::Settings *m_settings = nullptr;
    QVector<ConversationSummary> m_conversations;
    QVector<MessageRow> m_activeRows;
    MessageModel *m_activeMessageModel = nullptr;
    QVariantMap m_activeConversationData;
    QString m_activeConversationId;
    quint64 m_nextRequestId = 1;
    QHash<QString, QString> m_pendingMethods;
};
} // namespace StarryAgent
