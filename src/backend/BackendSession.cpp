#include "backend/BackendSession.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include "chat/Conversation.h"
#include "chat/ConversationManager.h"
#include "core/Settings.h"
#include "ipc/Protocol.h"

namespace StarryAgent
{
BackendSession::BackendSession(ConversationManager *conversationManager,
                               Settings *settings, QObject *parent)
    : QObject(parent), m_conversationManager(conversationManager),
      m_settings(settings)
{
    if (m_conversationManager)
    {
        connect(m_conversationManager, &ConversationManager::conversationsChanged,
                this, &BackendSession::emitConversationsReset);
        connect(m_conversationManager, &ConversationManager::activeChanged, this,
                &BackendSession::emitActiveConversationChanged);
        for (Conversation *conversation : m_conversationManager->list())
            observeConversation(conversation);
    }
    if (m_settings)
    {
        connect(m_settings, &Settings::apiBaseUrlChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::apiKeyChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::modelChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::modelsChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::streamingChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::bypassPermissionsChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::compactChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::themeChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::languageChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::currentThemeIdChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::webSearchImplementationChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::webSearchModelChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::webSearchExternalApiKeyChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::webSearchExternalBaseUrlChanged, this,
                &BackendSession::emitSettingsChanged);
        connect(m_settings, &Settings::globalScheduledTasksEnabledChanged, this,
                &BackendSession::emitSettingsChanged);
    }
}

QJsonObject BackendSession::bootstrap() const
{
    return Protocol::bootstrapPayload(m_conversationManager
                                          ? m_conversationManager->list()
                                          : QList<Conversation *>(),
                                      m_conversationManager
                                          ? m_conversationManager->active()
                                          : nullptr,
                                      m_settings);
}

QJsonObject BackendSession::handleRequest(const QJsonObject &request)
{
    const QString id = request.value(QString::fromLatin1(Protocol::kRequestId)).toString();
    const QString method =
        request.value(QString::fromLatin1(Protocol::kRequestMethod)).toString();
    const QJsonObject params =
        request.value(QString::fromLatin1(Protocol::kRequestParams)).toObject();

    if (method == QLatin1String(Protocol::kMethodAppBootstrap))
        return Protocol::makeResponse(id, bootstrap());

    if (method == QLatin1String(Protocol::kMethodConversationList))
    {
        QJsonArray items;
        const QList<Conversation *> conversations =
            m_conversationManager ? m_conversationManager->list() : QList<Conversation *>();
        for (Conversation *conversation : conversations)
        {
            if (!conversation)
                continue;
            items.append(Protocol::conversationSummary(
                *conversation,
                m_conversationManager && conversation == m_conversationManager->active()));
        }
        return Protocol::makeResponse(id, items);
    }

    if (method == QLatin1String(Protocol::kMethodSettingsGet))
        return Protocol::makeResponse(
            id, m_settings ? Protocol::settingsSnapshot(*m_settings) : QJsonObject());

    if (method == QLatin1String(Protocol::kMethodSettingsSet))
    {
        if (!m_settings)
        {
            return Protocol::makeResponse(
                id, QJsonValue(),
                Protocol::makeError(QStringLiteral("settings_unavailable"),
                                  QStringLiteral("settings unavailable")));
        }
        if (params.contains(QStringLiteral("apiBaseUrl")))
            m_settings->setApiBaseUrl(params.value(QStringLiteral("apiBaseUrl")).toString());
        if (params.contains(QStringLiteral("apiKey")))
            m_settings->setApiKey(params.value(QStringLiteral("apiKey")).toString());
        if (params.contains(QStringLiteral("model")))
            m_settings->setModel(params.value(QStringLiteral("model")).toString());
        if (params.contains(QStringLiteral("models")))
            m_settings->setModels(params.value(QStringLiteral("models")).toVariant().toStringList());
        if (params.contains(QStringLiteral("streaming")))
            m_settings->setStreaming(params.value(QStringLiteral("streaming")).toBool());
        if (params.contains(QStringLiteral("bypassPermissions")))
            m_settings->setBypassPermissions(params.value(QStringLiteral("bypassPermissions")).toBool());
        if (params.contains(QStringLiteral("compact")))
            m_settings->setCompact(params.value(QStringLiteral("compact")).toBool());
        if (params.contains(QStringLiteral("theme")))
            m_settings->setTheme(params.value(QStringLiteral("theme")).toString());
        if (params.contains(QStringLiteral("language")))
            m_settings->setLanguage(params.value(QStringLiteral("language")).toString());
        if (params.contains(QStringLiteral("currentThemeId")))
            m_settings->setCurrentThemeId(params.value(QStringLiteral("currentThemeId")).toString());
        if (params.contains(QStringLiteral("webSearchImplementation")))
            m_settings->setWebSearchImplementation(params.value(QStringLiteral("webSearchImplementation")).toString());
        if (params.contains(QStringLiteral("webSearchModel")))
            m_settings->setWebSearchModel(params.value(QStringLiteral("webSearchModel")).toString());
        if (params.contains(QStringLiteral("webSearchExternalApiKey")))
            m_settings->setWebSearchExternalApiKey(params.value(QStringLiteral("webSearchExternalApiKey")).toString());
        if (params.contains(QStringLiteral("webSearchExternalBaseUrl")))
            m_settings->setWebSearchExternalBaseUrl(params.value(QStringLiteral("webSearchExternalBaseUrl")).toString());
        if (params.contains(QStringLiteral("globalScheduledTasksEnabled")))
            m_settings->setGlobalScheduledTasksEnabled(params.value(QStringLiteral("globalScheduledTasksEnabled")).toBool());
        return Protocol::makeResponse(id, Protocol::settingsSnapshot(*m_settings));
    }

    Conversation *conversation = nullptr;
    if (params.contains(QStringLiteral("conversationId")))
        conversation = requireConversation(
            params.value(QStringLiteral("conversationId")).toString(), nullptr);
    else if (m_conversationManager)
        conversation = m_conversationManager->active();

    if (method == QLatin1String(Protocol::kMethodConversationCreate))
    {
        if (!m_conversationManager)
            return Protocol::makeResponse(
                id, QJsonValue(),
                Protocol::makeError(QStringLiteral("conversation_manager_unavailable"),
                                  QStringLiteral("conversation manager unavailable")));
        Conversation *created =
            m_conversationManager->newConversation(params.value(QStringLiteral("modeId")).toString());
        return Protocol::makeResponse(id, created ? created->id() : QString());
    }
    if (method == QLatin1String(Protocol::kMethodConversationRemove))
    {
        QJsonObject error;
        Conversation *target = requireConversation(
            params.value(QStringLiteral("conversationId")).toString(), &error);
        if (!target)
            return Protocol::makeResponse(id, QJsonValue(), error);
        m_conversationManager->remove(target);
        return Protocol::makeResponse(id, true);
    }
    if (method == QLatin1String(Protocol::kMethodConversationRename))
    {
        QJsonObject error;
        Conversation *target = requireConversation(
            params.value(QStringLiteral("conversationId")).toString(), &error);
        if (!target)
            return Protocol::makeResponse(id, QJsonValue(), error);
        m_conversationManager->rename(target,
                                      params.value(QStringLiteral("title")).toString());
        return Protocol::makeResponse(id, target->title());
    }
    if (method == QLatin1String(Protocol::kMethodConversationActivate))
    {
        QJsonObject error;
        Conversation *target = requireConversation(
            params.value(QStringLiteral("conversationId")).toString(), &error);
        if (!target)
            return Protocol::makeResponse(id, QJsonValue(), error);
        m_conversationManager->setActive(target);
        return Protocol::makeResponse(id, target->id());
    }

    if (!conversation)
    {
        return Protocol::makeResponse(
            id, QJsonValue(),
            Protocol::makeError(QStringLiteral("conversation_not_found"),
                              QStringLiteral("conversation not found")));
    }

    if (method == QLatin1String(Protocol::kMethodConversationSend))
    {
        conversation->sendWithImages(params.value(QStringLiteral("text")).toString(),
                                     params.value(QStringLiteral("imagePaths")).toVariant().toStringList());
        return Protocol::makeResponse(id, true);
    }
    if (method == QLatin1String(Protocol::kMethodConversationDispatchTool))
    {
        conversation->dispatch(params.value(QStringLiteral("toolCallId")).toString(),
                               params.value(QStringLiteral("name")).toString(),
                               params.value(QStringLiteral("argsText")).toString());
        return Protocol::makeResponse(id, true);
    }
    if (method == QLatin1String(Protocol::kMethodConversationDenyTool))
    {
        conversation->denyTool(params.value(QStringLiteral("toolCallId")).toString());
        return Protocol::makeResponse(id, true);
    }
    if (method == QLatin1String(Protocol::kMethodConversationCompact))
        return Protocol::makeResponse(
            id, conversation->compactNow(params.value(QStringLiteral("instructions")).toString()));
    if (method == QLatin1String(Protocol::kMethodConversationEnterPlanMode))
        return Protocol::makeResponse(
            id, conversation->enterPlanMode(params.value(QStringLiteral("note")).toString()));
    if (method == QLatin1String(Protocol::kMethodConversationSubmitPlan))
        return Protocol::makeResponse(id, conversation->submitPlanForApproval());
    if (method == QLatin1String(Protocol::kMethodConversationApprovePlan))
        return Protocol::makeResponse(id, conversation->approvePlan());
    if (method == QLatin1String(Protocol::kMethodConversationRejectPlan))
        return Protocol::makeResponse(
            id, conversation->rejectPlan(params.value(QStringLiteral("note")).toString()));
    if (method == QLatin1String(Protocol::kMethodConversationExitPlanMode))
        return Protocol::makeResponse(id, conversation->exitPlanMode());
    if (method == QLatin1String(Protocol::kMethodConversationShowPlan))
        return Protocol::makeResponse(id, conversation->showPlan());
    if (method == QLatin1String(Protocol::kMethodConversationClearPlan))
        return Protocol::makeResponse(id, conversation->clearPlan());
    if (method == QLatin1String(Protocol::kMethodConversationOpenPlan))
        return Protocol::makeResponse(id, conversation->openPlan());
    if (method == QLatin1String(Protocol::kMethodConversationShowWorkdir))
        return Protocol::makeResponse(id, conversation->showWorkdir());
    if (method == QLatin1String(Protocol::kMethodConversationSetWorkdir))
        return Protocol::makeResponse(
            id, conversation->setWorkdir(params.value(QStringLiteral("path")).toString()));
    if (method == QLatin1String(Protocol::kMethodConversationResetWorkdir))
        return Protocol::makeResponse(id, conversation->resetWorkdir());

    return Protocol::makeResponse(
        id, QJsonValue(),
        Protocol::makeError(QStringLiteral("unknown_method"), QStringLiteral("unknown method")));
}

void BackendSession::observeConversation(Conversation *conversation)
{
    if (!conversation)
        return;
    connect(conversation, &Conversation::titleChanged, this,
            &BackendSession::emitConversationsReset);
    connect(conversation, &Conversation::modeChanged, this,
            &BackendSession::emitConversationsReset);
    connect(conversation, &Conversation::updatedChanged, this,
            &BackendSession::emitConversationsReset);
    connect(conversation, &Conversation::streamingChanged, this,
            [this, conversation] { emitConversationStreamingChanged(conversation); });
    connect(conversation, &Conversation::errorChanged, this,
            [this, conversation] { emitConversationErrorChanged(conversation); });
    connect(conversation, &Conversation::planModeChanged, this,
            [this, conversation] { emitConversationPlanChanged(conversation); });
    connect(conversation, &Conversation::planTextChanged, this,
            [this, conversation] { emitConversationPlanChanged(conversation); });
    connect(conversation, &Conversation::planAwaitingApprovalChanged, this,
            [this, conversation] { emitConversationPlanChanged(conversation); });
    connect(conversation, &Conversation::workdirChanged, this,
            [this, conversation] { emitConversationWorkdirChanged(conversation); });
    connect(conversation, &QAbstractItemModel::rowsInserted, this,
            [this, conversation](const QModelIndex &, int first, int last)
            {
                emitConversationRowsInserted(conversation, first, last);
            });
    connect(conversation, &QAbstractItemModel::rowsRemoved, this,
            [this, conversation](const QModelIndex &, int first, int last)
            {
                emitConversationRowsRemoved(conversation, first, last);
            });
    connect(conversation, &QAbstractItemModel::dataChanged, this,
            [this, conversation](const QModelIndex &topLeft,
                                 const QModelIndex &bottomRight,
                                 const QList<int> &roles)
            {
                emitConversationRowsUpdated(conversation, topLeft.row(),
                                            bottomRight.row());
                if (roles.contains(Conversation::TextRole))
                    emitConversationToolRowsChanged(
                        conversation, QLatin1String(Protocol::kEventConversationDelta));
                if (roles.contains(Conversation::StatusRole) ||
                    roles.contains(Conversation::ResultRole))
                    emitConversationToolRowsChanged(
                        conversation, QLatin1String(Protocol::kEventConversationToolCallFinished));
                if (roles.contains(Conversation::ToolNameRole) ||
                    roles.contains(Conversation::ArgsTextRole) ||
                    roles.contains(Conversation::NeedsApprovalRole))
                    emitConversationToolRowsChanged(
                        conversation, QLatin1String(Protocol::kEventConversationToolCallReady));
            });
}

void BackendSession::emitConversationsReset()
{
    emit eventReady(Protocol::makeEvent(Protocol::kEventConversationsReset,
                                        bootstrap()));
}

void BackendSession::emitActiveConversationChanged()
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationActiveChanged,
        QJsonObject{{QStringLiteral("activeConversationId"),
                    m_conversationManager && m_conversationManager->active()
                        ? m_conversationManager->active()->id()
                        : QString()}}));
}

void BackendSession::emitConversationRowsInserted(Conversation *conversation, int first,
                                                  int last)
{
        QJsonObject payload{{QStringLiteral("conversationId"), conversation->id()},
                            {QStringLiteral("first"), first},
                            {QStringLiteral("last"), last},
                            {QStringLiteral("rows"), Protocol::conversationRows(*conversation)}};
        emit eventReady(Protocol::makeEvent(Protocol::kEventConversationRowsInserted,
                                            payload));
}

void BackendSession::emitConversationRowsUpdated(Conversation *conversation, int first,
                                                 int last)
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationRowsUpdated,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("first"), first},
                    {QStringLiteral("last"), last},
                    {QStringLiteral("rows"), Protocol::conversationRows(*conversation)}}));
}

void BackendSession::emitConversationRowsRemoved(Conversation *conversation, int first,
                                                 int last)
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationRowsRemoved,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("first"), first},
                    {QStringLiteral("last"), last},
                    {QStringLiteral("rows"), Protocol::conversationRows(*conversation)}}));
}

void BackendSession::emitConversationStreamingChanged(Conversation *conversation)
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationStreamingChanged,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("streaming"), conversation->streaming()}}));
    emitConversationToolRowsChanged(conversation,
                                    QLatin1String(Protocol::kEventConversationReasoningDelta));
}

void BackendSession::emitConversationToolRowsChanged(Conversation *conversation,
                                                    const QString &eventName)
{
    emit eventReady(Protocol::makeEvent(
        eventName,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("rows"), Protocol::conversationRows(*conversation)}}));
}

void BackendSession::emitConversationErrorChanged(Conversation *conversation)
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationErrorChanged,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("error"), conversation->error()}}));
}

void BackendSession::emitConversationPlanChanged(Conversation *conversation)
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationPlanChanged,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("planMode"), conversation->planMode()},
                    {QStringLiteral("planText"), conversation->planText()},
                    {QStringLiteral("planAwaitingApproval"),
                     conversation->planAwaitingApproval()}}));
}

void BackendSession::emitConversationWorkdirChanged(Conversation *conversation)
{
    emit eventReady(Protocol::makeEvent(
        Protocol::kEventConversationWorkdirChanged,
        QJsonObject{{QStringLiteral("conversationId"), conversation->id()},
                    {QStringLiteral("workdir"), conversation->workdir()}}));
}

void BackendSession::emitSettingsChanged()
{
    if (!m_settings)
        return;
    emit eventReady(Protocol::makeEvent(Protocol::kEventSettingsChanged,
                                       Protocol::settingsSnapshot(*m_settings)));
}

Conversation *BackendSession::requireConversation(const QString &conversationId,
                                                  QJsonObject *errorOut) const
{
    if (!m_conversationManager)
    {
        if (errorOut)
            *errorOut = Protocol::makeError(
                QStringLiteral("conversation_manager_unavailable"),
                QStringLiteral("conversation manager unavailable"));
        return nullptr;
    }
    Conversation *conversation = m_conversationManager->findById(conversationId);
    if (!conversation && errorOut)
        *errorOut = Protocol::makeError(QStringLiteral("conversation_not_found"),
                                        QStringLiteral("conversation not found"));
    return conversation;
}

} // namespace StarryAgent
