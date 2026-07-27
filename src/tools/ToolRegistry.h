#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>
#include <unordered_map>
#include <vector>

#include "Tool.h"

class Config;
class ScheduledTaskManager;
class Settings;

// ToolRegistry — owns the built-in tools, exposes them to QML, and dispatches
// approved tool calls on a worker thread.
//
// Loading: reads <root>/tools.jsonc. The `__built_in` entry toggles all
// built-in tools. Custom entries are supported:
//   - type:"cli" → launches a local command and passes tool args as JSON
//   - type:"mcp" → stdio MCP server; tools are discovered via tools/list
//
// Dispatch flow (Phase 4):
//   1. OpenAIClient emits toolCallReady(toolCallId, name, args).
//   2. QML ToolCallCard shows approve/deny (skipped if bypassPermissions).
//   3. User approves → QML calls registry.dispatch(toolCallId, name, args).
//   4. Registry runs tool->execute() on a worker thread.
//   5. Registry emits toolFinished(toolCallId, name, result).
//   6. QML appends a role:"tool" message and triggers the next send()
//      (the conversation loop closes in Phase 5 with ConversationManager).
class ToolRegistry : public QObject
{
    Q_OBJECT
  public:
    explicit ToolRegistry(Config *config, Settings *settings = nullptr,
                          QObject *parent = nullptr);

    // The `tools` array to send in an OpenAI request (enabled tools only).
    QJsonArray openaiToolsArray(bool schedulingAvailable = true) const;
    void setScheduledTaskManager(ScheduledTaskManager *manager);
    // True if a tool with this id is registered and enabled.
    Q_INVOKABLE bool exists(const QString &id) const;
    // Whether the tool requires per-call approval (for the ToolCallCard UI).
    Q_INVOKABLE bool permissionRequired(const QString &id) const;

    // Dispatch an approved tool call. Runs execute() on a worker thread;
    // emits toolFinished when done. Safe to call from QML.
    Q_INVOKABLE void dispatch(const QString &ownerId, const QString &toolCallId,
                              const QString &name, const QJsonObject &args,
                              const QString &workdir = QString());

  signals:
    void toolFinished(const QString &ownerId, const QString &toolCallId,
                      const QString &name, const QString &result);

  private:
    Tool *find(const QString &id) const;
    void loadBuiltIn();

    Config *m_config;
    Settings *m_settings = nullptr;
    std::vector<std::unique_ptr<Tool>> m_tools;
    std::unordered_map<QString, Tool *>
        m_byId; // non-owning; points into m_tools
    bool m_builtInEnabled = true;
    ScheduledTaskManager *m_scheduledTasks = nullptr;
    bool m_scheduledToolsRegistered = false;
};
