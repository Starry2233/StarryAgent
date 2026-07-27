#include "ScheduledTaskTools.h"

#include <QMetaObject>

#include "chat/ScheduledTaskManager.h"

using json = nlohmann::json;
namespace
{
QString text(const json &a, const char *key)
{
    return QString::fromStdString(a.value(key, std::string())).trimmed();
}
template <typename Callback>
QString onScheduler(ScheduledTaskManager *manager, Callback callback)
{
    if (!manager)
        return QStringLiteral("Error: scheduled task service is unavailable.");
    QString result;
    QMetaObject::invokeMethod(
        manager, [&] { result = callback(); }, Qt::BlockingQueuedConnection);
    return result;
}
} // namespace
QString CreateScheduledTaskTool::description() const
{
    return QStringLiteral(
        "Create an autonomous scheduled task for this conversation or "
        "globally. Prefer temp=true for one-time reminders/tasks, and use "
        "recurrence for repeating schedules. "
        "Unavailable in Coding mode. The response contains the unique task id "
        "needed for later management.");
}
json CreateScheduledTaskTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {{"prompt",
           {{"type", "string"},
            {"description",
             "Instruction delivered to the AI when the task fires (NOT a "
             "confirmation of task creation). Write it as the prompt the AI "
             "should act on at trigger time."}}},
          {"run_at",
           {{"type", "string"},
            {"description",
             "Future ISO-8601 local datetime, e.g. 2026-07-18T18:30:00."}}},
          {"recurrence",
           {{"type", "string"},
            {"enum", {"once", "daily", "weekly", "monthly", "interval"}},
            {"description",
             "Repeat rule. Omit for one-time tasks. Use interval only with "
             "interval_minutes."}}},
          {"interval_minutes",
           {{"type", "integer"},
            {"minimum", 0},
            {"description",
             "Repeat every N minutes when recurrence=interval."}}},
          {"temp",
           {{"type", "boolean"},
            {"description",
             "Temporary one-time task. Prefer true for one-time reminders "
             "that should disappear after running."}}},
          {"scope",
           {{"type", "string"},
            {"enum", {"conversation", "global"}},
            {"description", "Defaults to conversation. Global tasks survive "
                            "deleting their origin conversation."}}},
          {"id",
           {{"type", "string"},
            {"description",
             "Optional unique task id; normally omit and use the "
             "generated id."}}}}},
        {"required", {"prompt", "run_at"}}};
}
QString CreateScheduledTaskTool::execute(const json &args)
{
    return onScheduler(
        m_manager,
        [&]
        {
            return m_manager->createFromTool(
                text(args, "__conversation_id"), text(args, "prompt"),
                text(args, "scope"), text(args, "run_at"),
                args.value("interval_minutes", 0), text(args, "recurrence"),
                args.value("temp", false), text(args, "id"));
        });
}
QString SearchScheduledTasksTool::description() const
{
    return QStringLiteral(
        "Search scheduled tasks by id, prompt, or conversation "
        "title. Conversation scope is limited to this "
        "conversation; global tasks are shared.");
}
json SearchScheduledTasksTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {{"query",
           {{"type", "string"},
            {"description", "Text to search; omit to list tasks."}}},
          {"scope",
           {{"type", "string"},
            {"enum", {"conversation", "global"}},
            {"description", "Optional scope filter."}}},
          {"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 50}}}}}};
}
QString SearchScheduledTasksTool::execute(const json &args)
{
    return onScheduler(m_manager,
                       [&]
                       {
                           return m_manager->searchFromTool(
                               text(args, "__conversation_id"),
                               text(args, "query"), text(args, "scope"),
                               args.value("limit", 20));
                       });
}
QString DeleteScheduledTaskTool::description() const
{
    return QStringLiteral(
        "Delete a scheduled task by its unique id. A unique fuzzy match is "
        "accepted; search first if multiple tasks could match.");
}
json DeleteScheduledTaskTool::schema() const
{
    return {{"type", "object"},
            {"properties",
             {{"id_or_query",
               {{"type", "string"},
                {"description", "Task id or unique search phrase."}}}}},
            {"required", {"id_or_query"}}};
}
QString DeleteScheduledTaskTool::execute(const json &args)
{
    return onScheduler(m_manager,
                       [&]
                       {
                           return m_manager->deleteFromTool(
                               text(args, "__conversation_id"),
                               text(args, "id_or_query"));
                       });
}
