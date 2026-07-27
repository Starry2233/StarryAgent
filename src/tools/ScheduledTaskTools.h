#pragma once

#include "Tool.h"

class ScheduledTaskManager;

class CreateScheduledTaskTool final : public Tool
{
  public:
    explicit CreateScheduledTaskTool(ScheduledTaskManager *manager)
        : m_manager(manager)
    {
    }
    QString id() const override
    {
        return QStringLiteral("create_scheduled_task");
    }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    ScheduledTaskManager *m_manager;
};
class SearchScheduledTasksTool final : public Tool
{
  public:
    explicit SearchScheduledTasksTool(ScheduledTaskManager *manager)
        : m_manager(manager)
    {
    }
    QString id() const override
    {
        return QStringLiteral("search_scheduled_tasks");
    }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    ScheduledTaskManager *m_manager;
};
class DeleteScheduledTaskTool final : public Tool
{
  public:
    explicit DeleteScheduledTaskTool(ScheduledTaskManager *manager)
        : m_manager(manager)
    {
    }
    QString id() const override
    {
        return QStringLiteral("delete_scheduled_task");
    }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    ScheduledTaskManager *m_manager;
};
