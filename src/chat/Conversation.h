#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include "modes/Modes.h"

class OpenAIClient;
class ToolRegistry;
class Settings;

// One chat item in the flat flow: a user message, an assistant message, or a
// tool-call card. Tool calls are their own row (inline cards) — when building
// the OpenAI request, an assistant row followed by tool rows is collapsed
// into one assistant message carrying `tool_calls`, and each tool row becomes
// a `role:"tool"` result message.
//
// Each Conversation owns its own OpenAIClient so multiple conversations can
// stream in parallel without signal cross-talk or a shared in-flight guard.
class ConversationRuntime;

class Conversation : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString modeId READ modeId NOTIFY modeChanged)
    Q_PROPERTY(QDateTime created READ created CONSTANT)
    Q_PROPERTY(QDateTime updated READ updated NOTIFY updatedChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY streamingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool planMode READ planMode NOTIFY planModeChanged)
    Q_PROPERTY(QString planText READ planText NOTIFY planTextChanged)
    Q_PROPERTY(bool planAwaitingApproval READ planAwaitingApproval NOTIFY
                   planAwaitingApprovalChanged)
    Q_PROPERTY(QString workdir READ workdir NOTIFY workdirChanged)

  public:
    enum Roles
    {
        KindRole = Qt::UserRole + 1,
        TextRole,
        ToolCallIdRole,
        ToolNameRole,
        ArgsTextRole,
        StatusRole,
        ResultRole,
        NeedsApprovalRole,
        ImagePathRole,
        ImagePathsRole,
    };

    struct Item
    {
        QString kind;       // "user" | "assistant" | "tool"
        QString text;       // user/assistant text
        QString reasoningText; // assistant-only provider reasoning payload
        QString toolCallId; // tool items
        QString toolName;
        QString argsText; // pretty JSON for display + OpenAI arguments
        QString status;   // composing/pending/running/done/denied/error
        QString result;   // tool result content
        bool needsApproval = true;
        bool complete = true;
        QStringList imagePaths; // imported local images for user messages
    };

    explicit Conversation(Modes::Mode mode, QString indexMd,
                          QString skillsMd = {}, QObject *parent = nullptr);

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // --- properties ---
    QString id() const { return m_id; }
    QString title() const { return m_title; }
    void setTitle(const QString &t);
    QString modeId() const { return QString::fromLatin1(Modes::id(m_mode)); }
    Modes::Mode mode() const { return m_mode; }
    QDateTime created() const { return m_created; }
    QDateTime updated() const { return m_updated; }
    bool streaming() const { return m_streaming; }
    QString error() const { return m_error; }
    bool planMode() const { return m_planMode; }
    QString planText() const { return m_planText; }
    bool planAwaitingApproval() const { return m_planAwaitingApproval; }
    QString workdir() const { return m_workdir; }

    // Inject the per-conversation dependencies. Called once after construction
    // by ConversationManager (which owns the Settings/ToolRegistry singletons).
    void setContext(Settings *settings, ToolRegistry *registry);
    void setAttachmentsDir(QString attachmentsDir)
    {
        m_attachmentsDir = std::move(attachmentsDir);
    }
    void setPlanFilePath(QString planFilePath)
    {
        m_planFilePath = std::move(planFilePath);
    }
    void setDefaultWorkdir(QString workdir);

    // --- turn lifecycle (called by ChatView) ---
    // Append user + empty assistant, configure the client, kick off the
    // request.
    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void sendWithImages(const QString &text,
                                    const QStringList &imagePaths);
    // Executes an autonomous timer turn without adding a user bubble.
    void runScheduledTask(const QString &taskId, const QString &instruction);
    // Dispatch a tool call that the user approved (or auto-dispatched).
    Q_INVOKABLE void dispatch(const QString &toolCallId, const QString &name,
                              const QString &argsText);
    // User denied a tool call.
    Q_INVOKABLE void denyTool(const QString &toolCallId);
    Q_INVOKABLE QString importImage(const QString &sourcePath);

    // --- view mutators (called internally + by demo mode) ---
    Q_INVOKABLE void appendUser(const QString &text);
    Q_INVOKABLE void appendUserWithImages(const QString &text,
                                          const QStringList &imagePaths);
    Q_INVOKABLE void appendAssistant(); // empty row, streaming into it
    Q_INVOKABLE void appendAssistantText(const QString &text);
    Q_INVOKABLE void appendAssistantReasoningDelta(const QString &delta);
    Q_INVOKABLE void appendAssistantDelta(const QString &delta);
    Q_INVOKABLE void appendCompactBoundary(const QString &text);
    Q_INVOKABLE void appendToolCall(const QString &toolCallId,
                                    const QString &name);
    Q_INVOKABLE void setToolReady(const QString &toolCallId,
                                  const QString &name, const QString &argsText,
                                  bool needsApproval);
    Q_INVOKABLE void setToolRunning(const QString &toolCallId);
    Q_INVOKABLE void setToolDone(const QString &toolCallId,
                                 const QString &result);
    Q_INVOKABLE void setToolDenied(const QString &toolCallId);
    Q_INVOKABLE void setToolError(const QString &toolCallId,
                                  const QString &result);

    void setStreaming(bool on);

    // Build the OpenAI messages array (with system prompt prepended).
    Q_INVOKABLE QJsonArray buildMessages() const;

    // Persistence — serialize/parse to JSON for <root>/conversations/<id>.json
    QJsonObject toJson() const;
    static Conversation *fromJson(const QJsonObject &obj, QString indexMd,
                                  QString skillsMd = {},
                                  QObject *parent = nullptr);
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

  signals:
    void titleChanged();
    void modeChanged();
    void updatedChanged();
    void streamingChanged();
    void errorChanged();
    void planModeChanged();
    void planTextChanged();
    void planAwaitingApprovalChanged();
    void workdirChanged();
    void scheduledTaskFinished(const QString &taskId, bool success,
                               const QString &summary);

  private:
    struct BuildMessagesContext
    {
        QString workdir;
        QString compactSummary;
        int compactUntilRow = 0;
        bool planMode = false;
        QString planText;
        bool planAwaitingApproval = false;
        QString scheduledTaskId;
        QString scheduledInstruction;
    };

    class ConversationRuntime;

    QJsonArray buildMessagesFromRow(int startRow,
                                    const BuildMessagesContext &context) const;
    BuildMessagesContext buildMessagesContext() const;
    int firstLiveRow() const;
    int estimatedRequestTokens(const QString &pendingText = {},
                               const QStringList &pendingImages = {}) const;
    bool maybeAutoCompact(const QString &pendingText = {},
                          const QStringList &pendingImages = {},
                          bool reserveLatestUserTurn = false);
    int chooseCompactionCutRow() const;
    int chooseCompactionCutRow(int maxEndRowExclusive) const;
    QString buildCompactionTranscript(int cutRow) const;
    QString runCompactionRequest(const QString &transcript, QString *errorOut,
                                 const QString &instructions = QString()) const;
    void persistPlanFile() const;
    int indexOfTool(const QString &toolCallId) const;
    void touch(); // bump m_updated
    void appendRow(const Item &it);
    void updateRow(int i, const Item &it);
    void setError(const QString &msg);
    void syncClientSettings();
    void resendAfterToolResolution();
    void updatePlanTextFromLatestAssistant();

    // Lazily create + configure the per-conversation OpenAIClient and wire its
    // signals to this conversation's internal handlers.
    void ensureClient();

    // Signal handlers (connected to m_client / m_registry).
    void onClientDelta(const QString &text);
    void onClientReasoningDelta(const QString &text);
    void onClientComposing(const QString &id, const QString &name);
    void onClientToolName(const QString &id, const QString &name);
    void onClientReady(const QString &id, const QString &name,
                       const QJsonObject &args);
    void onClientFinished();
    void onClientError(const QString &msg);
    void onToolFinished(const QString &ownerId, const QString &id,
                        const QString &name, const QString &result);
    bool toolGroupReadyToContinue(int toolIndex) const;
    void maybeContinueAfterToolResolution(int toolIndex);
    QString resolveRequestedWorkdir(const QString &path) const;
    bool hasUnresolvedToolCalls() const;
    void finishScheduledTask(bool success);
    bool schedulingToolsAvailableForCurrentTurn() const;
    void scheduleAssistantDeltaFlush();
    void flushPendingAssistantDeltas();

    QString m_id;
    QString m_title;
    Modes::Mode m_mode;
    QString m_indexMd;
    QString m_skillsMd;
    QDateTime m_created;
    QDateTime m_updated;
    bool m_streaming = false;
    QString m_error;
    bool m_planMode = false;
    QString m_planText;
    bool m_planAwaitingApproval = false;
    QVector<Item> m_items;

    Settings *m_settings = nullptr;
    ToolRegistry *m_registry = nullptr;
    ConversationRuntime *m_runtime = nullptr;
    OpenAIClient *m_client = nullptr;
    bool m_pendingResend =
        false; // tool finished before the stream's finished() — defer resend
    bool m_clientFinished = true;
    QString m_attachmentsDir;
    QString m_planFilePath;
    QString m_defaultWorkdir;
    QString m_workdir;
    QString m_compactSummary;
    int m_compactUntilRow = 0;
    bool m_compactRetryAttempted = false;
    QString m_scheduledTaskId;
    QString m_scheduledInstruction;
    QString m_pendingAssistantText;
    QString m_pendingAssistantReasoningText;
    bool m_assistantDeltaFlushScheduled = false;
};
