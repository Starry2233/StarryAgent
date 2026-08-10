#include "ui/BackendStore.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ipc/BackendProcessTransport.h"
#include "ipc/Protocol.h"

namespace StarryAgent
{
class BackendStore::MessageModel : public QAbstractListModel
{
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
        ImagePathsRole,
    };

    explicit MessageModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
            return {};
        const MessageRow &row = m_rows.at(index.row());
        switch (role)
        {
        case KindRole: return row.kind;
        case TextRole: return row.text;
        case ToolCallIdRole: return row.toolCallId;
        case ToolNameRole: return row.toolName;
        case ArgsTextRole: return row.argsText;
        case StatusRole: return row.status;
        case ResultRole: return row.result;
        case NeedsApprovalRole: return row.needsApproval;
        case ImagePathsRole: return row.imagePaths;
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {{KindRole, "kind"}, {TextRole, "text"},
                {ToolCallIdRole, "toolCallId"}, {ToolNameRole, "toolName"},
                {ArgsTextRole, "argsText"}, {StatusRole, "status"},
                {ResultRole, "result"}, {NeedsApprovalRole, "needsApproval"},
                {ImagePathsRole, "imagePaths"}};
    }

    void resetRows(const QVector<MessageRow> &rows)
    {
        beginResetModel();
        m_rows = rows;
        endResetModel();
    }

  private:
    QVector<MessageRow> m_rows;
};

BackendStore::BackendStore(QObject *parent) : QAbstractListModel(parent)
{
    m_activeMessageModel = new MessageModel(this);
}

QAbstractItemModel *BackendStore::activeConversationModel() const
{
    return m_activeMessageModel;
}

QVariant BackendStore::activeConversation() const
{
    return m_activeConversationData;
}

int BackendStore::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_conversations.size();
}

QVariant BackendStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_conversations.size())
        return {};
    const ConversationSummary &conversation = m_conversations.at(index.row());
    switch (role)
    {
    case IdRole:
        return conversation.id;
    case TitleRole:
        return conversation.title;
    case ModeIdRole:
        return conversation.modeId;
    case CreatedRole:
        return conversation.created;
    case UpdatedRole:
        return conversation.updated;
    case StreamingRole:
        return conversation.streaming;
    case ErrorRole:
        return conversation.error;
    case PlanModeRole:
        return conversation.planMode;
    case PlanTextRole:
        return conversation.planText;
    case PlanAwaitingApprovalRole:
        return conversation.planAwaitingApproval;
    case WorkdirRole:
        return conversation.workdir;
    case ActiveRole:
        return conversation.active;
    case BucketRole:
    {
        const QDateTime updated = QDateTime::fromString(
            conversation.updated, Qt::ISODateWithMs);
        if (!updated.isValid())
            return QStringLiteral("Earlier");
        const QDateTime now = QDateTime::currentDateTime();
        if (updated.date() == now.date())
            return QStringLiteral("Today");
        if (updated.date() == now.addDays(-1).date())
            return QStringLiteral("Yesterday");
        if (updated.daysTo(now) <= 7)
            return QStringLiteral("Within 7 Days");
        return QStringLiteral("Earlier");
    }
    case RelativeTimeRole:
    {
        const QDateTime updated = QDateTime::fromString(
            conversation.updated, Qt::ISODateWithMs);
        if (!updated.isValid())
            return conversation.updated;
        const qint64 minutes = updated.secsTo(QDateTime::currentDateTime()) / 60;
        if (minutes < 1)
            return QStringLiteral("Just now");
        if (minutes < 60)
            return QStringLiteral("%1 minutes ago").arg(minutes);
        const qint64 hours = minutes / 60;
        if (hours < 24)
            return QStringLiteral("%1 hours ago").arg(hours);
        return updated.toString(QStringLiteral("MM-dd HH:mm"));
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> BackendStore::roleNames() const
{
    return {{IdRole, "id"},
            {TitleRole, "title"},
            {ModeIdRole, "modeId"},
            {CreatedRole, "created"},
            {UpdatedRole, "updated"},
            {StreamingRole, "streaming"},
            {ErrorRole, "error"},
            {PlanModeRole, "planMode"},
            {PlanTextRole, "planText"},
            {PlanAwaitingApprovalRole, "planAwaitingApproval"},
            {WorkdirRole, "workdir"},
            {ActiveRole, "active"},
            {BucketRole, "bucket"},
            {RelativeTimeRole, "relativeTime"}};
}

bool BackendStore::activeConversationStreaming() const
{
    const int row = conversationIndexById(m_activeConversationId);
    return row >= 0 ? m_conversations.at(row).streaming : false;
}

QString BackendStore::activeConversationError() const
{
    const int row = conversationIndexById(m_activeConversationId);
    return row >= 0 ? m_conversations.at(row).error : QString();
}

bool BackendStore::activeConversationPlanMode() const
{
    const int row = conversationIndexById(m_activeConversationId);
    return row >= 0 ? m_conversations.at(row).planMode : false;
}

QString BackendStore::activeConversationPlanText() const
{
    const int row = conversationIndexById(m_activeConversationId);
    return row >= 0 ? m_conversations.at(row).planText : QString();
}

bool BackendStore::activeConversationPlanAwaitingApproval() const
{
    const int row = conversationIndexById(m_activeConversationId);
    return row >= 0 ? m_conversations.at(row).planAwaitingApproval : false;
}

QString BackendStore::activeConversationWorkdir() const
{
    const int row = conversationIndexById(m_activeConversationId);
    return row >= 0 ? m_conversations.at(row).workdir : QString();
}

bool BackendStore::connected() const
{
    return m_transport && m_transport->isRunning();
}

void BackendStore::setTransport(BackendProcessTransport *transport)
{
    if (m_transport == transport)
        return;
    if (m_transport)
        disconnect(m_transport, nullptr, this, nullptr);
    m_transport = transport;
    if (m_transport)
    {
        connect(m_transport, &BackendProcessTransport::messageReceived, this,
                &BackendStore::onTransportMessage);
        connect(m_transport, &BackendProcessTransport::stopped, this,
                &BackendStore::onTransportStopped);
        connect(m_transport, &BackendProcessTransport::errorOccurred, this,
                [this](const QString &) { emit connectedChanged(); });
    }
    emit connectedChanged();
}

bool BackendStore::startLocalProcess(const QString &program,
                                     const QStringList &arguments)
{
    if (!m_transport)
        return false;
    m_transport->setProgram(program);
    m_transport->setArguments(arguments);
    if (!m_transport->start())
        return false;
    emit connectedChanged();
    return requestBootstrap();
}

bool BackendStore::requestBootstrap()
{
    return !sendTextCommand(QLatin1String(Protocol::kMethodAppBootstrap)).isEmpty();
}

void BackendStore::openNewConversation(const QString &modeId)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationCreate),
                    QJsonObject{{QStringLiteral("modeId"), modeId}});
}

void BackendStore::removeConversationById(const QString &conversationId)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationRemove),
                    QJsonObject{{QStringLiteral("conversationId"), conversationId}});
}

void BackendStore::renameConversationById(const QString &conversationId,
                                          const QString &title)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationRename),
                    QJsonObject{{QStringLiteral("conversationId"), conversationId},
                                {QStringLiteral("title"), title}});
}

void BackendStore::setActiveConversationById(const QString &conversationId)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationActivate),
                    QJsonObject{{QStringLiteral("conversationId"), conversationId}});
}

void BackendStore::sendWithImages(const QString &text, const QStringList &imagePaths)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationSend),
                    QJsonObject{{QStringLiteral("text"), text},
                                {QStringLiteral("imagePaths"), QJsonArray::fromStringList(imagePaths)}});
}

QString BackendStore::compactNow(const QString &instructions)
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationCompact),
                           QJsonObject{{QStringLiteral("instructions"), instructions}});
}

QString BackendStore::enterPlanMode(const QString &note)
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationEnterPlanMode),
                           QJsonObject{{QStringLiteral("note"), note}});
}

QString BackendStore::submitPlanForApproval()
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationSubmitPlan));
}

QString BackendStore::approvePlan()
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationApprovePlan));
}

QString BackendStore::rejectPlan(const QString &note)
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationRejectPlan),
                           QJsonObject{{QStringLiteral("note"), note}});
}

QString BackendStore::exitPlanMode()
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationExitPlanMode));
}

QString BackendStore::showPlan() const
{
    const int row = conversationIndexById(m_activeConversationId);
    if (row < 0)
        return QStringLiteral("No active plan.");
    const ConversationSummary &conversation = m_conversations.at(row);
    if (!conversation.planMode && conversation.planText.trimmed().isEmpty())
        return QStringLiteral("No active plan.");
    if (conversation.planText.trimmed().isEmpty())
        return QStringLiteral("Plan mode is enabled, but no plan has been written yet.");
    return conversation.planText;
}

QString BackendStore::clearPlan()
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationClearPlan));
}

QString BackendStore::openPlan()
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationOpenPlan));
}

QString BackendStore::showWorkdir() const
{
    const int row = conversationIndexById(m_activeConversationId);
    if (row < 0 || m_conversations.at(row).workdir.trimmed().isEmpty())
        return QStringLiteral("No working directory is configured for this conversation.");
    return QStringLiteral("Current workdir: %1").arg(m_conversations.at(row).workdir);
}

QString BackendStore::setWorkdir(const QString &path)
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationSetWorkdir),
                           QJsonObject{{QStringLiteral("path"), path}});
}

QString BackendStore::resetWorkdir()
{
    return sendTextCommand(QLatin1String(Protocol::kMethodConversationResetWorkdir));
}

void BackendStore::dispatch(const QString &toolCallId, const QString &name,
                            const QString &argsText)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationDispatchTool),
                    QJsonObject{{QStringLiteral("toolCallId"), toolCallId},
                                {QStringLiteral("name"), name},
                                {QStringLiteral("argsText"), argsText}});
}

void BackendStore::denyTool(const QString &toolCallId)
{
    sendTextCommand(QLatin1String(Protocol::kMethodConversationDenyTool),
                    QJsonObject{{QStringLiteral("toolCallId"), toolCallId}});
}

QString BackendStore::importImage(const QString &sourcePath)
{
    return sourcePath;
}

void BackendStore::appendAssistantText(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

    MessageRow row;
    row.kind = QStringLiteral("assistant");
    row.text = text;
    m_activeRows.append(row);
    if (m_activeMessageModel)
        m_activeMessageModel->resetRows(m_activeRows);
    emit activeConversationChanged();
    emit sessionsChanged();
}

void BackendStore::onTransportMessage(const QJsonObject &message)
{
    if (message.contains(QString::fromLatin1(Protocol::kEventName)))
    {
        const QString event = message.value(QString::fromLatin1(Protocol::kEventName)).toString();
        const QJsonObject payload = message.value(QString::fromLatin1(Protocol::kEventData)).toObject();
        if (event == QLatin1String(Protocol::kEventConversationsReset))
        {
            applyBootstrap(payload);
            return;
        }
        if (event == QLatin1String(Protocol::kEventConversationActiveChanged))
        {
            updateActiveConversationSummary(payload);
            return;
        }
        if (event == QLatin1String(Protocol::kEventConversationRowsInserted))
        {
            const QString conversationId = payload.value(QStringLiteral("conversationId")).toString();
            const int row = conversationIndexById(conversationId);
            if (row >= 0)
            {
                const int first = payload.value(QStringLiteral("first")).toInt();
                const int last = payload.value(QStringLiteral("last")).toInt();
                if (conversationId == m_activeConversationId)
                {
                    applyActiveRows(payload.value(QStringLiteral("rows")).toArray());
                    emit sessionsChanged();
                }
                else
                {
                    Q_UNUSED(first)
                    Q_UNUSED(last)
                }
            }
            return;
        }
        if (event == QLatin1String(Protocol::kEventConversationRowsUpdated))
        {
            const QString conversationId = payload.value(QStringLiteral("conversationId")).toString();
            const int row = conversationIndexById(conversationId);
            if (row >= 0)
            {
                if (conversationId == m_activeConversationId)
                    applyActiveRows(payload.value(QStringLiteral("rows")).toArray());
                else
                    emitConversationDataChanged(row);
                emit sessionsChanged();
            }
            return;
        }
        if (event == QLatin1String(Protocol::kEventConversationRowsRemoved))
        {
            const QString conversationId = payload.value(QStringLiteral("conversationId")).toString();
            const int row = conversationIndexById(conversationId);
            if (row >= 0)
            {
                if (conversationId == m_activeConversationId)
                    applyActiveRows(payload.value(QStringLiteral("rows")).toArray());
                emitConversationDataChanged(row);
                emit sessionsChanged();
            }
            return;
        }
        if (event == QLatin1String(Protocol::kEventConversationToolCallComposing) ||
            event == QLatin1String(Protocol::kEventConversationToolCallReady) ||
            event == QLatin1String(Protocol::kEventConversationToolCallFinished) ||
            event == QLatin1String(Protocol::kEventConversationDelta) ||
            event == QLatin1String(Protocol::kEventConversationReasoningDelta))
        {
            const QString conversationId = payload.value(QStringLiteral("conversationId")).toString();
            if (conversationId == m_activeConversationId)
            {
                applyActiveRows(payload.value(QStringLiteral("rows")).toArray());
                emit activeConversationChanged();
                emit sessionsChanged();
            }
            return;
        }
        if (event == QLatin1String(Protocol::kEventConversationStreamingChanged) ||
            event == QLatin1String(Protocol::kEventConversationErrorChanged) ||
            event == QLatin1String(Protocol::kEventConversationPlanChanged) ||
            event == QLatin1String(Protocol::kEventConversationWorkdirChanged) ||
            event == QLatin1String(Protocol::kEventSettingsChanged))
        {
            const QString conversationId = payload.value(QStringLiteral("conversationId")).toString();
            const int row = conversationIndexById(conversationId);
            if (row >= 0)
            {
                ConversationSummary &conversation = m_conversations[row];
                if (payload.contains(QStringLiteral("streaming")))
                    conversation.streaming = payload.value(QStringLiteral("streaming")).toBool();
                if (payload.contains(QStringLiteral("error")))
                    conversation.error = payload.value(QStringLiteral("error")).toString();
                if (payload.contains(QStringLiteral("planMode")))
                    conversation.planMode = payload.value(QStringLiteral("planMode")).toBool();
                if (payload.contains(QStringLiteral("planText")))
                    conversation.planText = payload.value(QStringLiteral("planText")).toString();
                if (payload.contains(QStringLiteral("planAwaitingApproval")))
                    conversation.planAwaitingApproval = payload.value(QStringLiteral("planAwaitingApproval")).toBool();
                if (payload.contains(QStringLiteral("workdir")))
                    conversation.workdir = payload.value(QStringLiteral("workdir")).toString();
                emit dataChanged(index(row), index(row));
                emit activeConversationChanged();
                emit sessionsChanged();
            }
            return;
        }
        return;
    }
    if (message.contains(QString::fromLatin1(Protocol::kResponseResult)) ||
        message.contains(QString::fromLatin1(Protocol::kResponseError)))
    {
        const QString id = message.value(QString::fromLatin1(Protocol::kRequestId)).toString();
        const QString method = m_pendingMethods.take(id);
        emit commandCompleted(id, method,
                             message.value(QString::fromLatin1(Protocol::kResponseResult)).toObject(),
                             message.value(QString::fromLatin1(Protocol::kResponseError)).toObject());
    }
}

void BackendStore::onTransportStopped()
{
    emit connectedChanged();
}

QVariantMap BackendStore::get(int row) const
{
    if (row < 0 || row >= m_conversations.size())
        return {};
    const QModelIndex modelIndex = index(row, 0);
    QVariantMap result;
    result.insert(QStringLiteral("id"), data(modelIndex, IdRole));
    result.insert(QStringLiteral("title"), data(modelIndex, TitleRole));
    result.insert(QStringLiteral("modeId"), data(modelIndex, ModeIdRole));
    result.insert(QStringLiteral("created"), data(modelIndex, CreatedRole));
    result.insert(QStringLiteral("updated"), data(modelIndex, UpdatedRole));
    result.insert(QStringLiteral("bucket"), data(modelIndex, BucketRole));
    result.insert(QStringLiteral("relativeTime"), data(modelIndex, RelativeTimeRole));
    result.insert(QStringLiteral("active"), data(modelIndex, ActiveRole));
    return result;
}

void BackendStore::saveActiveConversation()
{
    // Backend persistence is applied by each mutating protocol command.
}

void BackendStore::appendUser(const QString &text)
{
    sendWithImages(text, {});
}

QString BackendStore::nextRequestId()
{
    return QString::number(m_nextRequestId++);
}

bool BackendStore::sendRequest(const QString &method, const QJsonObject &params,
                               QString *requestIdOut)
{
    if (!m_transport)
        return false;
    const QString id = nextRequestId();
    QJsonObject request = Protocol::makeRequest(id, method, params);
    if (requestIdOut)
        *requestIdOut = id;
    m_pendingMethods.insert(id, method);
    return m_transport->sendMessage(request);
}

QString BackendStore::sendTextCommand(const QString &method, const QJsonObject &params)
{
    QString requestId;
    return sendRequest(method, params, &requestId) ? requestId : QString();
}

QJsonObject BackendStore::sendSyncRequest(const QString &method, const QJsonObject &params)
{
    Q_UNUSED(method)
    Q_UNUSED(params)
    return {};
}

void BackendStore::applyBootstrap(const QJsonObject &payload)
{
    beginResetModel();
    m_conversations.clear();
    m_activeRows.clear();
    m_activeConversationId = payload.value(QStringLiteral("activeConversationId")).toString();
    applyConversations(payload.value(QStringLiteral("conversations")).toArray());
    applyActiveRows(payload.value(QStringLiteral("activeRows")).toArray());
    m_activeConversationData.clear();
    const int activeRow = conversationIndexById(m_activeConversationId);
    if (activeRow >= 0)
    {
        const ConversationSummary &summary = m_conversations.at(activeRow);
        m_activeConversationData.insert(QStringLiteral("id"), summary.id);
        m_activeConversationData.insert(QStringLiteral("title"), summary.title);
        m_activeConversationData.insert(QStringLiteral("modeId"), summary.modeId);
        m_activeConversationData.insert(QStringLiteral("created"), summary.created);
        m_activeConversationData.insert(QStringLiteral("updated"), summary.updated);
    }
    endResetModel();
    emit countChanged();
    emit activeConversationChanged();
    emit sessionsChanged();
}

void BackendStore::applyConversations(const QJsonArray &items)
{
    for (const QJsonValue &value : items)
        upsertConversation(conversationFromJson(value.toObject()));
}

void BackendStore::applyActiveRows(const QJsonArray &rows)
{
    m_activeRows.clear();
    for (const QJsonValue &value : rows)
        m_activeRows.append(messageFromJson(value.toObject()));
    if (m_activeMessageModel)
        m_activeMessageModel->resetRows(m_activeRows);
}

void BackendStore::updateActiveConversationSummary(const QJsonObject &patch)
{
    const QString conversationId = patch.value(QStringLiteral("activeConversationId")).toString();
    if (!conversationId.isEmpty())
        m_activeConversationId = conversationId;
    emit activeConversationChanged();
}

int BackendStore::conversationIndexById(const QString &conversationId) const
{
    for (int i = 0; i < m_conversations.size(); ++i)
        if (m_conversations[i].id == conversationId)
            return i;
    return -1;
}

BackendStore::ConversationSummary BackendStore::conversationFromJson(const QJsonObject &obj) const
{
    ConversationSummary summary;
    summary.id = obj.value(QStringLiteral("id")).toString();
    summary.title = obj.value(QStringLiteral("title")).toString();
    summary.modeId = obj.value(QStringLiteral("modeId")).toString();
    summary.created = obj.value(QStringLiteral("created")).toString();
    summary.updated = obj.value(QStringLiteral("updated")).toString();
    summary.streaming = obj.value(QStringLiteral("streaming")).toBool();
    summary.error = obj.value(QStringLiteral("error")).toString();
    summary.planMode = obj.value(QStringLiteral("planMode")).toBool();
    summary.planText = obj.value(QStringLiteral("planText")).toString();
    summary.planAwaitingApproval = obj.value(QStringLiteral("planAwaitingApproval")).toBool();
    summary.workdir = obj.value(QStringLiteral("workdir")).toString();
    summary.active = obj.value(QStringLiteral("active")).toBool();
    return summary;
}

BackendStore::MessageRow BackendStore::messageFromJson(const QJsonObject &obj) const
{
    MessageRow row;
    row.kind = obj.value(QStringLiteral("kind")).toString();
    row.text = obj.value(QStringLiteral("text")).toString();
    row.toolCallId = obj.value(QStringLiteral("toolCallId")).toString();
    row.toolName = obj.value(QStringLiteral("toolName")).toString();
    row.argsText = obj.value(QStringLiteral("argsText")).toString();
    row.status = obj.value(QStringLiteral("status")).toString();
    row.result = obj.value(QStringLiteral("result")).toString();
    row.needsApproval = obj.value(QStringLiteral("needsApproval")).toBool();
    row.imagePaths = obj.value(QStringLiteral("imagePaths")).toVariant().toStringList();
    return row;
}

void BackendStore::upsertConversation(const ConversationSummary &summary)
{
    const int row = conversationIndexById(summary.id);
    if (row < 0)
    {
        beginInsertRows(QModelIndex(), m_conversations.size(), m_conversations.size());
        m_conversations.append(summary);
        endInsertRows();
        emit countChanged();
        return;
    }
    m_conversations[row] = summary;
    emit dataChanged(index(row), index(row));
}

void BackendStore::emitConversationDataChanged(int row)
{
    if (row >= 0 && row < m_conversations.size())
        emit dataChanged(index(row), index(row));
}

} // namespace StarryAgent
