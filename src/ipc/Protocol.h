#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

class Conversation;
class Settings;

namespace StarryAgent::Protocol
{

inline constexpr const char kRequestId[] = "id";
inline constexpr const char kRequestMethod[] = "method";
inline constexpr const char kRequestParams[] = "params";
inline constexpr const char kResponseResult[] = "result";
inline constexpr const char kResponseError[] = "error";
inline constexpr const char kEventName[] = "event";
inline constexpr const char kEventData[] = "data";

inline constexpr const char kMethodAppBootstrap[] = "app.bootstrap";
inline constexpr const char kMethodConversationList[] = "conversation.list";
inline constexpr const char kMethodConversationCreate[] = "conversation.create";
inline constexpr const char kMethodConversationRemove[] = "conversation.remove";
inline constexpr const char kMethodConversationRename[] = "conversation.rename";
inline constexpr const char kMethodConversationActivate[] = "conversation.activate";
inline constexpr const char kMethodConversationSend[] = "conversation.send";
inline constexpr const char kMethodConversationDispatchTool[] = "conversation.dispatchTool";
inline constexpr const char kMethodConversationDenyTool[] = "conversation.denyTool";
inline constexpr const char kMethodConversationCompact[] = "conversation.compact";
inline constexpr const char kMethodConversationEnterPlanMode[] = "conversation.enterPlanMode";
inline constexpr const char kMethodConversationSubmitPlan[] = "conversation.submitPlan";
inline constexpr const char kMethodConversationApprovePlan[] = "conversation.approvePlan";
inline constexpr const char kMethodConversationRejectPlan[] = "conversation.rejectPlan";
inline constexpr const char kMethodConversationExitPlanMode[] = "conversation.exitPlanMode";
inline constexpr const char kMethodConversationShowPlan[] = "conversation.showPlan";
inline constexpr const char kMethodConversationClearPlan[] = "conversation.clearPlan";
inline constexpr const char kMethodConversationOpenPlan[] = "conversation.openPlan";
inline constexpr const char kMethodConversationShowWorkdir[] = "conversation.showWorkdir";
inline constexpr const char kMethodConversationSetWorkdir[] = "conversation.setWorkdir";
inline constexpr const char kMethodConversationResetWorkdir[] = "conversation.resetWorkdir";
inline constexpr const char kMethodSettingsGet[] = "settings.get";
inline constexpr const char kMethodSettingsSet[] = "settings.set";

inline constexpr const char kEventConversationsReset[] = "conversations.reset";
inline constexpr const char kEventConversationActiveChanged[] = "conversation.activeChanged";
inline constexpr const char kEventConversationRowsInserted[] = "conversation.rowsInserted";
inline constexpr const char kEventConversationRowsUpdated[] = "conversation.rowsUpdated";
inline constexpr const char kEventConversationRowsRemoved[] = "conversation.rowsRemoved";
inline constexpr const char kEventConversationStreamingChanged[] = "conversation.streamingChanged";
inline constexpr const char kEventConversationErrorChanged[] = "conversation.errorChanged";
inline constexpr const char kEventConversationPlanChanged[] = "conversation.planChanged";
inline constexpr const char kEventConversationWorkdirChanged[] = "conversation.workdirChanged";
inline constexpr const char kEventConversationToolCallComposing[] = "conversation.toolCallComposing";
inline constexpr const char kEventConversationToolCallReady[] = "conversation.toolCallReady";
inline constexpr const char kEventConversationToolCallFinished[] = "conversation.toolCallFinished";
inline constexpr const char kEventConversationDelta[] = "conversation.delta";
inline constexpr const char kEventConversationReasoningDelta[] = "conversation.reasoningDelta";
inline constexpr const char kEventSettingsChanged[] = "settings.changed";

inline QJsonObject makeRequest(const QString &id, const QString &method,
                               const QJsonObject &params = {})
{
    QJsonObject obj;
    obj.insert(QString::fromLatin1(kRequestId), id);
    obj.insert(QString::fromLatin1(kRequestMethod), method);
    obj.insert(QString::fromLatin1(kRequestParams), params);
    return obj;
}

QJsonObject makeError(const QString &code, const QString &message,
                      const QJsonObject &details = {});
QJsonObject makeResponse(const QString &id,
                         const QJsonValue &result = QJsonValue(),
                         const QJsonObject &error = {});
QJsonObject makeEvent(const QString &event,
                      const QJsonValue &data = QJsonValue(QJsonObject()));
QJsonObject conversationSummary(const Conversation &conversation, bool active);
QJsonObject conversationRow(const Conversation &conversation, int row);
QJsonArray conversationRows(const Conversation &conversation);
QJsonObject settingsSnapshot(const Settings &settings);
QJsonObject bootstrapPayload(const QList<Conversation *> &conversations,
                             const Conversation *activeConversation,
                             const Settings *settings);

inline QJsonObject wrapStringList(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    QJsonObject obj;
    obj.insert(QStringLiteral("values"), array);
    return obj;
}

} // namespace StarryAgent::Protocol
