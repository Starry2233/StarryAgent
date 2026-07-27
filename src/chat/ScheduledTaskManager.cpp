#include "ScheduledTaskManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include "ConversationManager.h"
#include "core/Config.h"
#include "core/Settings.h"

namespace
{
QString iso(const QDateTime &value)
{
    return value.isValid() ? value.toLocalTime().toString(Qt::ISODate)
                           : QString();
}
QDateTime parseTime(const QString &value)
{
    return QDateTime::fromString(value, Qt::ISODate);
}
QString recurrenceLabel(const QString &recurrence, int intervalMinutes,
                        bool temp)
{
    if (temp)
        return QStringLiteral("temp");
    if (recurrence == "interval" && intervalMinutes > 0)
        return QStringLiteral("every %1 minutes").arg(intervalMinutes);
    return recurrence;
}
} // namespace

ScheduledTaskManager::ScheduledTaskManager(Config *config, Settings *settings,
                                           ConversationManager *conversations,
                                           QObject *parent)
    : QAbstractListModel(parent), m_config(config), m_settings(settings),
      m_conversations(conversations)
{
    load();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this,
            &ScheduledTaskManager::checkDueTasks);
    m_timer.start();
    QTimer::singleShot(0, this, &ScheduledTaskManager::checkDueTasks);
}

int ScheduledTaskManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tasks.size();
}
QVariant ScheduledTaskManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tasks.size())
        return {};
    const Task &t = m_tasks.at(index.row());
    switch (role)
    {
    case IdRole:
        return t.id;
    case PromptRole:
        return t.prompt;
    case ScopeRole:
        return t.scope;
    case ConversationIdRole:
        return t.conversationId;
    case ConversationTitleRole:
        return t.conversationTitle;
    case NextRunRole:
        return iso(t.nextRun);
    case IntervalMinutesRole:
        return t.intervalMinutes;
    case RecurrenceRole:
        return t.recurrence;
    case TempRole:
        return t.temp;
    case EnabledRole:
        return t.enabled;
    case StatusRole:
        return t.status;
    case LastRunRole:
        return iso(t.lastRun);
    case LastResultRole:
        return t.lastResult;
    default:
        return {};
    }
}
QHash<int, QByteArray> ScheduledTaskManager::roleNames() const
{
    return {{IdRole, "taskId"},
            {PromptRole, "prompt"},
            {ScopeRole, "scope"},
            {ConversationIdRole, "conversationId"},
            {ConversationTitleRole, "conversationTitle"},
            {NextRunRole, "nextRun"},
            {IntervalMinutesRole, "intervalMinutes"},
            {RecurrenceRole, "recurrence"},
            {TempRole, "temp"},
            {EnabledRole, "enabled"},
            {StatusRole, "status"},
            {LastRunRole, "lastRun"},
            {LastResultRole, "lastResult"}};
}
bool ScheduledTaskManager::globalEnabled() const
{
    return m_settings && m_settings->globalScheduledTasksEnabled();
}
void ScheduledTaskManager::setGlobalEnabled(bool enabled)
{
    if (m_settings)
        m_settings->setGlobalScheduledTasksEnabled(enabled);
    emit globalEnabledChanged();
}

QJsonObject ScheduledTaskManager::toJson(const Task &t)
{
    return {{"id", t.id},
            {"prompt", t.prompt},
            {"scope", t.scope},
            {"conversationId", t.conversationId},
            {"conversationTitle", t.conversationTitle},
            {"nextRun", iso(t.nextRun)},
            {"intervalMinutes", t.intervalMinutes},
            {"recurrence", t.recurrence},
            {"temp", t.temp},
            {"enabled", t.enabled},
            {"status", t.status},
            {"lastRun", iso(t.lastRun)},
            {"lastResult", t.lastResult}};
}
ScheduledTaskManager::Task ScheduledTaskManager::fromJson(const QJsonObject &j)
{
    Task t;
    t.id = j.value("id").toString();
    t.prompt = j.value("prompt").toString();
    t.scope = j.value("scope").toString("conversation");
    t.conversationId = j.value("conversationId").toString();
    t.conversationTitle = j.value("conversationTitle").toString();
    t.nextRun = parseTime(j.value("nextRun").toString());
    t.intervalMinutes = j.value("intervalMinutes").toInt();
    t.recurrence = normalizeRecurrence(j.value("recurrence").toString(),
                                       t.intervalMinutes);
    t.temp = j.value("temp").toBool(false);
    t.enabled = j.value("enabled").toBool(true);
    t.status = j.value("status").toString();
    t.lastRun = parseTime(j.value("lastRun").toString());
    t.lastResult = j.value("lastResult").toString();
    return t;
}
void ScheduledTaskManager::load()
{
    if (!m_config)
        return;
    QFile file(m_config->scheduledTasksPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &value : array)
    {
        Task t = fromJson(value.toObject());
        if (!t.id.isEmpty() && !t.prompt.isEmpty() && t.nextRun.isValid())
            m_tasks.append(t);
    }
}
void ScheduledTaskManager::save() const
{
    if (!m_config)
        return;
    QJsonArray array;
    for (const Task &t : m_tasks)
        array.append(toJson(t));
    QFile file(m_config->scheduledTasksPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}
int ScheduledTaskManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_tasks.size(); ++i)
        if (m_tasks[i].id == id)
            return i;
    return -1;
}
bool ScheduledTaskManager::isGlobalAllowed(const Task &task) const
{
    return task.scope != "global" || globalEnabled();
}

QString ScheduledTaskManager::createFromTool(
    const QString &conversationId, const QString &prompt, const QString &scope,
    const QString &runAt, int intervalMinutes, const QString &recurrence,
    bool temp, const QString &requestedId)
{
    const QString cleanPrompt = prompt.trimmed();
    const QString cleanScope = scope.trimmed().toLower().isEmpty()
                                   ? QStringLiteral("conversation")
                                   : scope.trimmed().toLower();
    const QString cleanRecurrence =
        normalizeRecurrence(recurrence.trimmed().toLower(), intervalMinutes);
    const QDateTime date = parseTime(runAt);
    if (cleanPrompt.isEmpty())
        return "Error: prompt is required.";
    if (cleanScope != "conversation" && cleanScope != "global")
        return "Error: scope must be conversation or global.";
    if (!date.isValid())
        return "Error: run_at must be an ISO-8601 local datetime, e.g. "
               "2026-07-18T18:30:00.";
    if (date <= QDateTime::currentDateTime().addSecs(-5))
        return "Error: run_at must be in the future.";
    if (intervalMinutes < 0)
        return "Error: interval_minutes cannot be negative.";
    if (cleanRecurrence.isEmpty())
        return "Error: recurrence must be once, daily, weekly, monthly, or "
               "interval.";
    if (cleanRecurrence == "interval" && intervalMinutes <= 0)
        return "Error: recurrence=interval requires interval_minutes > 0.";
    if (cleanRecurrence != "interval" && intervalMinutes > 0)
        return "Error: interval_minutes is only valid with recurrence=interval.";
    if (temp && cleanRecurrence != "once")
        return "Error: temp tasks must be one-time tasks.";
    if (cleanScope == "conversation")
    {
        int count = 0;
        for (const Task &t : m_tasks)
            if (t.scope == "conversation" &&
                t.conversationId == conversationId && t.enabled)
                ++count;
        if (count >= 20)
            return "Error: this conversation already has 20 enabled scheduled "
                   "tasks.";
    }
    Conversation *conversation =
        m_conversations ? m_conversations->findById(conversationId) : nullptr;
    if (!conversation)
        return "Error: the originating conversation no longer exists.";
    if (conversation->modeId() == "coding")
        return "Error: scheduled tasks are unavailable in Coding mode.";
    QString id = requestedId.trimmed();
    if (id.isEmpty())
        id = "task_" +
             QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    if (indexOf(id) >= 0)
        return "Error: task id already exists.";
    beginInsertRows({}, m_tasks.size(), m_tasks.size());
    m_tasks.append({id,
                    cleanPrompt,
                    cleanScope,
                    conversationId,
                    conversation->title(),
                    date,
                    intervalMinutes,
                    cleanRecurrence,
                    temp,
                    true,
                    "scheduled",
                    {},
                    {}});
    endInsertRows();
    save();
    emit tasksChanged();
    return QStringLiteral("Scheduled task created. "
                          "id=%1\nscope=%2\nnext_run=%3\nrecurrence=%4\n"
                          "interval_minutes=%5\ntemp=%6\nwarning: scheduled "
                          "task runs with full tool permissions by default, "
                          "regardless of the Bypass permissions setting.")
        .arg(id, cleanScope, iso(date), cleanRecurrence)
        .arg(intervalMinutes)
        .arg(temp ? "true" : "false");
}
QString ScheduledTaskManager::formattedTask(const Task &t) const
{
    return QStringLiteral(
               "id: %1\nscope: %2\nconversation: %3\nenabled: %4\nnext_run: "
               "%5\nrecurrence: %6\ninterval_minutes: %7\ntemp: %8\nstatus: "
               "%9\nprompt: %10")
        .arg(t.id, t.scope, t.conversationTitle, t.enabled ? "true" : "false",
             iso(t.nextRun))
        .arg(t.recurrence)
        .arg(t.intervalMinutes)
        .arg(t.temp ? "true" : "false")
        .arg(t.status, t.prompt);
}
QString ScheduledTaskManager::searchFromTool(const QString &conversationId,
                                             const QString &query,
                                             const QString &scope,
                                             int limit) const
{
    const QString needle = query.trimmed().toLower();
    const QString wanted = scope.trimmed().toLower();
    QStringList results;
    limit = qBound(1, limit, 50);
    for (const Task &t : m_tasks)
    {
        if (!wanted.isEmpty() && t.scope != wanted)
            continue;
        if (t.scope == "conversation" && t.conversationId != conversationId)
            continue;
        const QString hay =
            (t.id + "\n" + t.prompt + "\n" + t.conversationTitle).toLower();
        if (!needle.isEmpty() && !hay.contains(needle))
            continue;
        results << formattedTask(t);
        if (results.size() >= limit)
            break;
    }
    return results.isEmpty() ? QStringLiteral("No scheduled tasks matched.")
                             : QStringLiteral("Scheduled tasks:\n\n%1")
                                   .arg(results.join("\n\n"));
}
QString ScheduledTaskManager::resolveTaskId(const QString &idOrQuery,
                                            const QString &conversationId) const
{
    if (indexOf(idOrQuery) >= 0)
        return idOrQuery;
    const QString needle = idOrQuery.trimmed().toLower();
    QString match;
    for (const Task &t : m_tasks)
    {
        if (t.scope == "conversation" && t.conversationId != conversationId)
            continue;
        if ((t.id + "\n" + t.prompt).toLower().contains(needle))
        {
            if (!match.isEmpty())
                return {};
            match = t.id;
        }
    }
    return match;
}
QString ScheduledTaskManager::deleteFromTool(const QString &conversationId,
                                             const QString &idOrQuery)
{
    const QString id = resolveTaskId(idOrQuery, conversationId);
    if (id.isEmpty())
        return "Error: no unique scheduled task matched; search first and use "
               "its "
               "id.";
    deleteTask(id);
    return QStringLiteral("Scheduled task deleted: %1").arg(id);
}
void ScheduledTaskManager::setTaskEnabled(const QString &id, bool enabled)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    Task &task = m_tasks[i];
    if (enabled && !task.enabled && task.scope == "conversation")
    {
        int activeCount = 0;
        for (const Task &other : m_tasks)
            if (other.scope == "conversation" &&
                other.conversationId == task.conversationId && other.enabled)
                ++activeCount;
        if (activeCount >= 20)
        {
            emit taskNotification(
                QStringLiteral("Scheduled task not enabled"),
                QStringLiteral("A conversation can have at most 20 enabled "
                               "scheduled tasks."));
            return;
        }
    }
    task.enabled = enabled;
    task.status = enabled ? "scheduled" : "disabled";
    emit dataChanged(index(i), index(i));
    save();
    emit tasksChanged();
}
void ScheduledTaskManager::deleteTask(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    beginRemoveRows({}, i, i);
    m_tasks.removeAt(i);
    endRemoveRows();
    save();
    emit tasksChanged();
}
void ScheduledTaskManager::removeConversationTasks(
    const QString &conversationId)
{
    for (int i = m_tasks.size() - 1; i >= 0; --i)
        if (m_tasks[i].scope == "conversation" &&
            m_tasks[i].conversationId == conversationId)
        {
            beginRemoveRows({}, i, i);
            m_tasks.removeAt(i);
            endRemoveRows();
        }
    save();
    emit tasksChanged();
}
QString ScheduledTaskManager::taskSummary() const
{
    return QStringLiteral("%1 scheduled tasks").arg(m_tasks.size());
}
QString ScheduledTaskManager::normalizeRecurrence(const QString &recurrence,
                                                  int intervalMinutes)
{
    if (recurrence.isEmpty())
        return intervalMinutes > 0 ? QStringLiteral("interval")
                                   : QStringLiteral("once");
    if (recurrence == "once" || recurrence == "daily" ||
        recurrence == "weekly" || recurrence == "monthly")
        return recurrence;
    if (recurrence == "interval")
        return QStringLiteral("interval");
    return {};
}
QDateTime ScheduledTaskManager::computeNextRun(const Task &task,
                                               const QDateTime &baseTime)
{
    if (!baseTime.isValid())
        return {};
    const QDateTime now = QDateTime::currentDateTime();
    if (task.recurrence == "interval" && task.intervalMinutes > 0)
    {
        QDateTime next = baseTime;
        do
        {
            next = next.addSecs(task.intervalMinutes * 60);
        } while (next <= now);
        return next;
    }
    QDateTime next = baseTime;
    if (task.recurrence == "daily")
    {
        do
        {
            next = next.addDays(1);
        } while (next <= now);
        return next;
    }
    if (task.recurrence == "weekly")
    {
        do
        {
            next = next.addDays(7);
        } while (next <= now);
        return next;
    }
    if (task.recurrence == "monthly")
    {
        do
        {
            next = next.addMonths(1);
        } while (next <= now);
        return next;
    }
    return {};
}
void ScheduledTaskManager::checkDueTasks()
{
    const QDateTime now = QDateTime::currentDateTime();
    for (Task &t : m_tasks)
        if (t.enabled && isGlobalAllowed(t) && t.status != "running" &&
            t.nextRun <= now)
            trigger(t);
}
void ScheduledTaskManager::trigger(Task &task)
{
    if (!m_conversations)
        return;
    Conversation *c = m_conversations->findById(task.conversationId);
    if (!c && task.scope == "global")
    {
        c = m_conversations->newConversation("agent");
        c->setTitle(QStringLiteral("Scheduled task %1").arg(task.id));
        task.conversationId = c->id();
        task.conversationTitle = c->title();
    }
    if (!c)
    {
        task.enabled = false;
        task.status = "orphaned";
        save();
        emit tasksChanged();
        return;
    }
    if (c->streaming())
    {
        task.nextRun = QDateTime::currentDateTime().addSecs(30);
        task.status = "waiting for conversation";
        save();
        emit tasksChanged();
        return;
    }
    task.status = "running";
    task.lastRun = QDateTime::currentDateTime();
    const int row = indexOf(task.id);
    emit dataChanged(index(row), index(row));
    save();
    c->runScheduledTask(task.id, task.prompt);
}
void ScheduledTaskManager::completeRun(const QString &taskId, bool success,
                                       const QString &result)
{
    const int i = indexOf(taskId);
    if (i < 0)
        return;
    Task &t = m_tasks[i];
    t.status = success ? "completed" : "failed";
    t.lastResult = result.left(1200);
    const QDateTime nextRun = computeNextRun(t, t.nextRun.isValid() ? t.nextRun
                                                                    : t.lastRun);
    if (nextRun.isValid())
    {
        t.nextRun = nextRun;
        t.status = "scheduled";
    }
    else if (t.temp)
    {
        const QString title = t.conversationTitle;
        const QString resultPreview = t.lastResult.left(180);
        beginRemoveRows({}, i, i);
        m_tasks.removeAt(i);
        endRemoveRows();
        save();
        emit tasksChanged();
        emit taskNotification(success ? QStringLiteral("Scheduled task completed")
                                      : QStringLiteral("Scheduled task failed"),
                              QStringLiteral("%1\n%2").arg(title,
                                                           resultPreview));
        return;
    }
    else
        t.enabled = false;
    emit dataChanged(index(i), index(i));
    save();
    emit tasksChanged();
    emit taskNotification(success ? QStringLiteral("Scheduled task completed")
                                  : QStringLiteral("Scheduled task failed"),
                          QStringLiteral("%1\n%2").arg(t.conversationTitle,
                                                       t.lastResult.left(180)));
}
