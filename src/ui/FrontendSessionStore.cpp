#include "FrontendSessionStore.h"

#include <QAbstractItemModel>
#include <QDir>
#include <QModelIndex>

#include "chat/Conversation.h"
#include "chat/ConversationManager.h"

FrontendSessionStore::FrontendSessionStore(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FrontendSessionStore::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant FrontendSessionStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    switch (role)
    {
    case ConversationRole:
        return QVariant::fromValue(row.conversation);
    case IdRole:
        return row.id;
    case TitleRole:
        return row.title;
    case ModeIdRole:
        return row.modeId;
    case CreatedRole:
        return row.created;
    case UpdatedRole:
        return row.updated;
    case BucketRole:
        return row.bucket;
    case RelativeTimeRole:
        return row.relativeTime;
    case ActiveRole:
        return row.active;
    }
    return {};
}

QHash<int, QByteArray> FrontendSessionStore::roleNames() const
{
    return {
        {ConversationRole, "conversation"},
        {IdRole, "id"},
        {TitleRole, "title"},
        {ModeIdRole, "modeId"},
        {CreatedRole, "created"},
        {UpdatedRole, "updated"},
        {BucketRole, "bucket"},
        {RelativeTimeRole, "relativeTime"},
        {ActiveRole, "active"},
    };
}

void FrontendSessionStore::setConversationManager(ConversationManager *manager)
{
    if (m_conversationManager == manager)
        return;

    for (const Row &row : std::as_const(m_rows))
        unobserveConversation(row.conversation);

    if (m_conversationManager)
        disconnect(m_conversationManager, nullptr, this, nullptr);

    m_conversationManager = manager;
    if (m_conversationManager)
    {
        connect(m_conversationManager, &ConversationManager::conversationsChanged,
                this, &FrontendSessionStore::syncFromManager);
        connect(m_conversationManager, &ConversationManager::activeChanged, this,
                &FrontendSessionStore::syncFromManager);
        connect(m_conversationManager, &QObject::destroyed, this,
                [this]()
                {
                    m_conversationManager = nullptr;
                    syncFromManager();
                    emit conversationManagerChanged();
                });
    }

    syncFromManager();
    emit conversationManagerChanged();
}

int FrontendSessionStore::count() const
{
    return m_rows.size();
}

QAbstractItemModel *FrontendSessionStore::activeConversationModel() const
{
    return m_activeConversation;
}

bool FrontendSessionStore::activeConversationStreaming() const
{
    return m_activeConversation && m_activeConversation->streaming();
}

QString FrontendSessionStore::activeConversationError() const
{
    return m_activeConversation ? m_activeConversation->error() : QString();
}

Conversation *FrontendSessionStore::conversationAt(int index) const
{
    if (index < 0 || index >= m_rows.size())
        return nullptr;
    return m_rows.at(index).conversation;
}

Conversation *FrontendSessionStore::conversationById(const QString &id) const
{
    if (!m_conversationManager)
        return nullptr;
    return m_conversationManager->findById(id);
}

QVariantMap FrontendSessionStore::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};

    const QModelIndex modelIndex = index(row, 0);
    QVariantMap result;
    result.insert(QStringLiteral("conversation"), data(modelIndex, ConversationRole));
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

void FrontendSessionStore::setActiveConversation(Conversation *conversation)
{
    if (!m_conversationManager)
        return;
    m_conversationManager->setActive(conversation);
}

void FrontendSessionStore::setActiveConversationById(const QString &id)
{
    if (!m_conversationManager)
        return;
    if (Conversation *conversation = m_conversationManager->findById(id))
        m_conversationManager->setActive(conversation);
}

void FrontendSessionStore::openNewConversation(const QString &modeId)
{
    if (!m_conversationManager)
        return;
    m_conversationManager->newConversation(modeId);
}

void FrontendSessionStore::removeConversation(Conversation *conversation)
{
    if (!m_conversationManager)
        return;
    m_conversationManager->remove(conversation);
}

void FrontendSessionStore::removeConversationById(const QString &id)
{
    if (!m_conversationManager)
        return;
    if (Conversation *conversation = m_conversationManager->findById(id))
        m_conversationManager->remove(conversation);
}

void FrontendSessionStore::renameConversation(Conversation *conversation,
                                              const QString &newTitle)
{
    if (!m_conversationManager)
        return;
    m_conversationManager->rename(conversation, newTitle);
}

void FrontendSessionStore::renameConversationById(const QString &id,
                                                  const QString &newTitle)
{
    if (!m_conversationManager)
        return;
    if (Conversation *conversation = m_conversationManager->findById(id))
        m_conversationManager->rename(conversation, newTitle);
}

void FrontendSessionStore::saveActiveConversation()
{
    if (!m_conversationManager)
        return;
    m_conversationManager->saveActive();
}

void FrontendSessionStore::sendWithImages(const QString &text, const QStringList &imagePaths)
{
    if (m_activeConversation)
        m_activeConversation->sendWithImages(text, imagePaths);
}

void FrontendSessionStore::appendAssistantText(const QString &text)
{
    if (m_activeConversation)
        m_activeConversation->appendAssistantText(text);
}

void FrontendSessionStore::appendUser(const QString &text)
{
    if (m_activeConversation)
        m_activeConversation->appendUser(text);
}

QString FrontendSessionStore::compactNow(const QString &instructions)
{
    return m_activeConversation ? m_activeConversation->compactNow(instructions) : QString();
}

QString FrontendSessionStore::enterPlanMode(const QString &note)
{
    return m_activeConversation ? m_activeConversation->enterPlanMode(note) : QString();
}

QString FrontendSessionStore::submitPlanForApproval()
{
    return m_activeConversation ? m_activeConversation->submitPlanForApproval() : QString();
}

QString FrontendSessionStore::approvePlan()
{
    return m_activeConversation ? m_activeConversation->approvePlan() : QString();
}

QString FrontendSessionStore::rejectPlan(const QString &note)
{
    return m_activeConversation ? m_activeConversation->rejectPlan(note) : QString();
}

QString FrontendSessionStore::exitPlanMode()
{
    return m_activeConversation ? m_activeConversation->exitPlanMode() : QString();
}

QString FrontendSessionStore::showPlan() const
{
    return m_activeConversation ? m_activeConversation->showPlan() : QString();
}

QString FrontendSessionStore::clearPlan()
{
    return m_activeConversation ? m_activeConversation->clearPlan() : QString();
}

QString FrontendSessionStore::openPlan()
{
    return m_activeConversation ? m_activeConversation->openPlan() : QString();
}

QString FrontendSessionStore::showWorkdir() const
{
    return m_activeConversation ? m_activeConversation->showWorkdir() : QString();
}

QString FrontendSessionStore::setWorkdir(const QString &path)
{
    return m_activeConversation ? m_activeConversation->setWorkdir(path) : QString();
}

QString FrontendSessionStore::resetWorkdir()
{
    return m_activeConversation ? m_activeConversation->resetWorkdir() : QString();
}

QString FrontendSessionStore::importImage(const QString &sourcePath)
{
    return m_activeConversation ? m_activeConversation->importImage(sourcePath) : QString();
}

void FrontendSessionStore::dispatch(const QString &toolCallId, const QString &name,
                                    const QString &argsText)
{
    if (m_activeConversation)
        m_activeConversation->dispatch(toolCallId, name, argsText);
}

void FrontendSessionStore::denyTool(const QString &toolCallId)
{
    if (m_activeConversation)
        m_activeConversation->denyTool(toolCallId);
}

void FrontendSessionStore::syncFromManager()
{
    beginResetModel();
    rebuildRows();
    endResetModel();
    refreshActiveConversation();
    emit sessionsChanged();
}

void FrontendSessionStore::observeConversation(Conversation *conversation)
{
    if (!conversation)
        return;

    auto refreshRow = [this, conversation]()
    {
        const int row = rowIndexForConversation(conversation);
        if (row < 0)
            return;

        Row &entry = m_rows[row];
        entry.title = conversation->title();
        entry.modeId = conversation->modeId();
        entry.created = conversation->created();
        entry.updated = conversation->updated();
        entry.bucket = bucketOf(entry.updated);
        entry.relativeTime = relativeTimeOf(entry.updated);
        entry.active = m_conversationManager && m_conversationManager->active() == conversation;
        emit dataChanged(index(row), index(row));
        emit sessionsChanged();
    };

    connect(conversation, &Conversation::titleChanged, this, refreshRow);
    connect(conversation, &Conversation::modeChanged, this, refreshRow);
    connect(conversation, &Conversation::updatedChanged, this, refreshRow);
    connect(conversation, &QAbstractItemModel::rowsInserted, this,
            [this, conversation, refreshRow]()
            {
                if (conversation == m_activeConversation)
                    emit activeConversationChanged();
                refreshRow();
            });
    connect(conversation, &QAbstractItemModel::rowsRemoved, this,
            [this, conversation, refreshRow]()
            {
                if (conversation == m_activeConversation)
                    emit activeConversationChanged();
                refreshRow();
            });
    connect(conversation, &QAbstractItemModel::modelReset, this,
            [this, conversation, refreshRow]()
            {
                if (conversation == m_activeConversation)
                    emit activeConversationChanged();
                refreshRow();
            });
    connect(conversation, &QAbstractItemModel::dataChanged, this,
            [this, conversation](const QModelIndex &, const QModelIndex &,
                                 const QList<int> &)
            {
                if (conversation == m_activeConversation)
                    emit activeConversationChanged();
            });
}

void FrontendSessionStore::unobserveConversation(Conversation *conversation)
{
    if (conversation)
        disconnect(conversation, nullptr, this, nullptr);
}

void FrontendSessionStore::rebuildRows()
{
    for (const Row &row : std::as_const(m_rows))
        unobserveConversation(row.conversation);

    m_rows.clear();
    if (!m_conversationManager)
        return;

    const QList<Conversation *> list = m_conversationManager->list();
    m_rows.reserve(list.size());
    for (Conversation *conversation : list)
    {
        if (!conversation)
            continue;
        observeConversation(conversation);
        Row row;
        row.conversation = conversation;
        row.id = conversation->id();
        row.title = conversation->title();
        row.modeId = conversation->modeId();
        row.created = conversation->created();
        row.updated = conversation->updated();
        row.bucket = bucketOf(row.updated);
        row.relativeTime = relativeTimeOf(row.updated);
        row.active = m_conversationManager->active() == conversation;
        m_rows.append(row);
    }
}

void FrontendSessionStore::refreshActiveConversation()
{
    for (const QMetaObject::Connection &connection :
         std::as_const(m_activeConversationConnections))
    {
        disconnect(connection);
    }
    m_activeConversationConnections.clear();

    Conversation *nextActive = m_conversationManager ? m_conversationManager->active() : nullptr;
    if (m_activeConversation == nextActive)
    {
        if (m_activeConversation)
        {
            m_activeConversationConnections.append(connect(
                m_activeConversation, &Conversation::streamingChanged, this,
                &FrontendSessionStore::activeConversationChanged));
            m_activeConversationConnections.append(connect(
                m_activeConversation, &Conversation::errorChanged, this,
                &FrontendSessionStore::activeConversationChanged));
            m_activeConversationConnections.append(connect(
                m_activeConversation, &QAbstractItemModel::rowsInserted, this,
                &FrontendSessionStore::activeConversationChanged));
            m_activeConversationConnections.append(connect(
                m_activeConversation, &QAbstractItemModel::rowsRemoved, this,
                &FrontendSessionStore::activeConversationChanged));
            m_activeConversationConnections.append(connect(
                m_activeConversation, &QAbstractItemModel::modelReset, this,
                &FrontendSessionStore::activeConversationChanged));
            m_activeConversationConnections.append(connect(
                m_activeConversation, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &, const QModelIndex &,
                       const QList<int> &)
                { emit activeConversationChanged(); }));
            m_activeConversationConnections.append(connect(
                m_activeConversation, &QObject::destroyed, this,
                [this]()
                {
                    m_activeConversation = nullptr;
                    m_activeConversationConnections.clear();
                    emit activeConversationChanged();
                }));
        }
        return;
    }
    m_activeConversation = nextActive;
    if (m_activeConversation)
    {
        m_activeConversationConnections.append(connect(
            m_activeConversation, &Conversation::streamingChanged, this,
            &FrontendSessionStore::activeConversationChanged));
        m_activeConversationConnections.append(connect(
            m_activeConversation, &Conversation::errorChanged, this,
            &FrontendSessionStore::activeConversationChanged));
        m_activeConversationConnections.append(connect(
            m_activeConversation, &QAbstractItemModel::rowsInserted, this,
            &FrontendSessionStore::activeConversationChanged));
        m_activeConversationConnections.append(connect(
            m_activeConversation, &QAbstractItemModel::rowsRemoved, this,
            &FrontendSessionStore::activeConversationChanged));
        m_activeConversationConnections.append(connect(
            m_activeConversation, &QAbstractItemModel::modelReset, this,
            &FrontendSessionStore::activeConversationChanged));
        m_activeConversationConnections.append(connect(
            m_activeConversation, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &, const QModelIndex &,
                   const QList<int> &)
            { emit activeConversationChanged(); }));
        m_activeConversationConnections.append(connect(
            m_activeConversation, &QObject::destroyed, this,
            [this]()
            {
                m_activeConversation = nullptr;
                m_activeConversationConnections.clear();
                emit activeConversationChanged();
            }));
    }
    emit activeConversationChanged();
}

QString FrontendSessionStore::bucketOf(const QDateTime &updated) const
{
    const QDateTime now = QDateTime::currentDateTime();
    if (updated.date() == now.date())
        return QStringLiteral("Today");
    if (updated.date() == now.addDays(-1).date())
        return QStringLiteral("Yesterday");
    if (updated.daysTo(now) <= 7)
        return QStringLiteral("Within 7 Days");
    return QStringLiteral("Earlier");
}

QString FrontendSessionStore::relativeTimeOf(const QDateTime &updated) const
{
    const qint64 mins = updated.secsTo(QDateTime::currentDateTime()) / 60;
    if (mins < 1)
        return QStringLiteral("Just now");
    if (mins < 60)
        return QStringLiteral("%1 minutes ago").arg(mins);
    const qint64 hrs = mins / 60;
    if (hrs < 24)
        return QStringLiteral("%1 hours ago").arg(hrs);
    return updated.toString(QStringLiteral("MM-dd HH:mm"));
}

int FrontendSessionStore::rowIndexForConversation(Conversation *conversation) const
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows.at(i).conversation == conversation)
            return i;
    }
    return -1;
}
