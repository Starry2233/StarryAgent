#include "ipc/Protocol.h"

#include <QDateTime>
#include <QJsonValue>
#include <QModelIndex>

#include "chat/Conversation.h"
#include "core/Settings.h"

namespace StarryAgent::Protocol
{
namespace
{
QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}
} // namespace

QJsonObject makeError(const QString &code, const QString &message,
                      const QJsonObject &details)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("code"), code);
    obj.insert(QStringLiteral("message"), message);
    if (!details.isEmpty())
        obj.insert(QStringLiteral("details"), details);
    return obj;
}

QJsonObject makeResponse(const QString &id, const QJsonValue &result,
                         const QJsonObject &error)
{
    QJsonObject obj;
    obj.insert(QString::fromLatin1(kRequestId), id);
    if (!result.isUndefined())
        obj.insert(QString::fromLatin1(kResponseResult), result);
    if (!error.isEmpty())
        obj.insert(QString::fromLatin1(kResponseError), error);
    return obj;
}

QJsonObject makeEvent(const QString &event, const QJsonValue &data)
{
    QJsonObject obj;
    obj.insert(QString::fromLatin1(kEventName), event);
    if (!data.isUndefined())
        obj.insert(QString::fromLatin1(kEventData), data);
    return obj;
}

QJsonObject conversationSummary(const Conversation &conversation, bool active)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), conversation.id());
    obj.insert(QStringLiteral("title"), conversation.title());
    obj.insert(QStringLiteral("modeId"), conversation.modeId());
    obj.insert(QStringLiteral("created"),
               conversation.created().toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("updated"),
               conversation.updated().toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("streaming"), conversation.streaming());
    obj.insert(QStringLiteral("error"), conversation.error());
    obj.insert(QStringLiteral("planMode"), conversation.planMode());
    obj.insert(QStringLiteral("planText"), conversation.planText());
    obj.insert(QStringLiteral("planAwaitingApproval"),
               conversation.planAwaitingApproval());
    obj.insert(QStringLiteral("workdir"), conversation.workdir());
    obj.insert(QStringLiteral("active"), active);
    return obj;
}

QJsonObject conversationRow(const Conversation &conversation, int row)
{
    QJsonObject obj;
    const QModelIndex index = conversation.index(row, 0);
    obj.insert(QStringLiteral("row"), row);
    obj.insert(QStringLiteral("kind"),
               conversation.data(index, Conversation::KindRole).toString());
    obj.insert(QStringLiteral("text"),
               conversation.data(index, Conversation::TextRole).toString());
    obj.insert(QStringLiteral("toolCallId"),
               conversation.data(index, Conversation::ToolCallIdRole).toString());
    obj.insert(QStringLiteral("toolName"),
               conversation.data(index, Conversation::ToolNameRole).toString());
    obj.insert(QStringLiteral("argsText"),
               conversation.data(index, Conversation::ArgsTextRole).toString());
    obj.insert(QStringLiteral("status"),
               conversation.data(index, Conversation::StatusRole).toString());
    obj.insert(QStringLiteral("result"),
               conversation.data(index, Conversation::ResultRole).toString());
    obj.insert(QStringLiteral("needsApproval"),
               conversation.data(index, Conversation::NeedsApprovalRole).toBool());

    const QVariant imagePaths =
        conversation.data(index, Conversation::ImagePathsRole);
    if (imagePaths.isValid())
        obj.insert(QStringLiteral("imagePaths"),
                   QJsonArray::fromStringList(imagePaths.toStringList()));
    return obj;
}

QJsonArray conversationRows(const Conversation &conversation)
{
    QJsonArray rows;
    for (int row = 0; row < conversation.rowCount(); ++row)
        rows.append(conversationRow(conversation, row));
    return rows;
}

QJsonObject settingsSnapshot(const Settings &settings)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("apiBaseUrl"), settings.apiBaseUrl());
    obj.insert(QStringLiteral("apiKey"), settings.apiKey());
    obj.insert(QStringLiteral("model"), settings.model());
    obj.insert(QStringLiteral("models"), stringListToJson(settings.models()));
    obj.insert(QStringLiteral("streaming"), settings.streaming());
    obj.insert(QStringLiteral("bypassPermissions"), settings.bypassPermissions());
    obj.insert(QStringLiteral("compact"), settings.compact());
    obj.insert(QStringLiteral("theme"), settings.theme());
    obj.insert(QStringLiteral("language"), settings.language());
    obj.insert(QStringLiteral("currentThemeId"), settings.currentThemeId());
    obj.insert(QStringLiteral("webSearchImplementation"),
               settings.webSearchImplementation());
    obj.insert(QStringLiteral("webSearchModel"), settings.webSearchModel());
    obj.insert(QStringLiteral("webSearchExternalApiKey"),
               settings.webSearchExternalApiKey());
    obj.insert(QStringLiteral("webSearchExternalBaseUrl"),
               settings.webSearchExternalBaseUrl());
    obj.insert(QStringLiteral("globalScheduledTasksEnabled"),
               settings.globalScheduledTasksEnabled());
    return obj;
}

QJsonObject bootstrapPayload(const QList<Conversation *> &conversations,
                             const Conversation *activeConversation,
                             const Settings *settings)
{
    QJsonArray items;
    for (Conversation *conversation : conversations)
    {
        if (!conversation)
            continue;
        items.append(conversationSummary(*conversation,
                                         conversation == activeConversation));
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversations"), items);
    obj.insert(QStringLiteral("activeConversationId"),
               activeConversation ? activeConversation->id() : QString());
    if (activeConversation)
        obj.insert(QStringLiteral("activeRows"),
                   conversationRows(*activeConversation));
    else
        obj.insert(QStringLiteral("activeRows"), QJsonArray());
    if (settings)
        obj.insert(QStringLiteral("settings"), settingsSnapshot(*settings));
    return obj;
}

} // namespace StarryAgent::Protocol
