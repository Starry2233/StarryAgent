#include "ToolRegistry.h"

#include <QJsonDocument>
#include <QMetaObject>
#include <QProcessEnvironment>

#include <nlohmann/json.hpp>

#include <thread>
#include <utility>

#include "BroadcastTool.h"
#include "CliCustomTool.h"
#include "EditTool.h"
#include "ExecTool.h"
#include "LoadSkillTool.h"
#include "McpTool.h"
#include "OverwriteTool.h"
#include "ReadSkillReferenceTool.h"
#include "RecallMemoryTool.h"
#include "RootExecTool.h"
#include "ScheduledTaskTools.h"
#include "ShellExecTool.h"
#include "Sqlite3Tool.h"
#include "WebDownloadTool.h"
#include "WebFetchTool.h"
#include "WebSearchTool.h"
#include "WriteMemoryTool.h"
#include "core/Config.h"
#include "core/DebugTrace.h"
#include "core/Settings.h"

using json = nlohmann::json;

namespace
{
QString stripJsonc(const QString &text)
{
    QString out;
    out.reserve(text.size());
    bool inString = false;
    bool escape = false;
    for (int i = 0; i < text.size(); ++i)
    {
        const QChar ch = text[i];
        if (escape)
        {
            out.append(ch);
            escape = false;
            continue;
        }
        if (ch == QLatin1Char('\\'))
        {
            out.append(ch);
            escape = true;
            continue;
        }
        if (ch == QLatin1Char('"'))
        {
            inString = !inString;
            out.append(ch);
            continue;
        }
        if (!inString && ch == QLatin1Char('/') && i + 1 < text.size() &&
            text[i + 1] == QLatin1Char('/'))
        {
            while (i < text.size() && text[i] != QLatin1Char('\n'))
                ++i;
            if (i < text.size())
                out.append(text[i]);
            continue;
        }
        out.append(ch);
    }
    return out;
}

QStringList jsonStringList(const json &value)
{
    QStringList out;
    if (!value.is_array())
        return out;
    for (const auto &entry : value)
    {
        if (entry.is_string())
            out.append(QString::fromStdString(entry.get<std::string>()));
    }
    return out;
}

QProcessEnvironment jsonEnv(const json &value)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!value.is_object())
        return env;
    for (const auto &[key, entry] : value.items())
    {
        if (entry.is_string())
            env.insert(QString::fromStdString(key),
                       QString::fromStdString(entry.get<std::string>()));
    }
    return env;
}
} // namespace

ToolRegistry::ToolRegistry(Config *config, Settings *settings, QObject *parent)
    : QObject(parent), m_config(config), m_settings(settings)
{
    loadBuiltIn();
}

void ToolRegistry::loadBuiltIn()
{
    if (!m_config)
        return;

    json parsed = json::array();
    QByteArray raw = m_config->loadToolsJsonc();
    if (raw.isEmpty())
    {
        m_builtInEnabled = true;
    }
    else
    {
        try
        {
            parsed =
                json::parse(stripJsonc(QString::fromUtf8(raw)).toStdString());
            if (parsed.is_array())
            {
                for (const auto &entry : parsed)
                {
                    const std::string id = entry.value("id", std::string());
                    const bool enabled = entry.value("enabled", true);
                    if (id == "__built_in")
                        m_builtInEnabled = enabled;
                }
            }
        }
        catch (...)
        {
            m_builtInEnabled = true;
            parsed = json::array();
        }
    }

    const QString workspace = m_config->workspacePath();
    auto registerTool = [this](std::unique_ptr<Tool> t)
    {
        m_byId[t->id()] = t.get();
        m_tools.push_back(std::move(t));
    };
    if (m_builtInEnabled)
    {
        registerTool(std::make_unique<EditTool>(workspace));
        registerTool(std::make_unique<OverwriteTool>(workspace));
        registerTool(std::make_unique<ExecTool>(workspace));
        registerTool(std::make_unique<ShellExecTool>(workspace));
        registerTool(std::make_unique<WebFetchTool>());
        registerTool(std::make_unique<WebSearchTool>(m_settings));
        registerTool(std::make_unique<WebDownloadTool>(workspace));
        registerTool(std::make_unique<Sqlite3Tool>(workspace));
        registerTool(std::make_unique<RootExecTool>(workspace));
        registerTool(std::make_unique<BroadcastTool>());
        registerTool(
            std::make_unique<RecallMemoryTool>(m_config->memoriesPath()));
        registerTool(
            std::make_unique<WriteMemoryTool>(m_config->memoriesPath()));
        m_skillManager =
            std::make_unique<SkillManager>(m_config->skillsPath(), m_settings);
        registerTool(std::make_unique<LoadSkillTool>(m_skillManager.get()));
        registerTool(
            std::make_unique<ReadSkillReferenceTool>(m_skillManager.get()));
    }

    if (!parsed.is_array())
        return;

    for (const auto &entry : parsed)
    {
        if (!entry.is_object())
            continue;
        if (entry.value("id", std::string()) == "__built_in")
            continue;
        if (!entry.value("enabled", true) || !entry.value("custom", false))
            continue;

        const QString publicId =
            QString::fromStdString(entry.value("id", std::string()));
        const QString type =
            QString::fromStdString(entry.value("type", std::string()))
                .trimmed()
                .toLower();
        const json config = entry.value("config", json::object());
        const QString description = QString::fromStdString(entry.value(
            "description", config.value("description", std::string())));
        const json schema =
            entry.contains("schema")
                ? entry["schema"]
                : config.value("schema", json{{"type", "object"},
                                              {"properties", json::object()}});
        const bool approvalRequired = entry.value(
            "permission_required", config.value("permission_required", true));

        if (type == QStringLiteral("cli"))
        {
            const QString command =
                QString::fromStdString(config.value("command", std::string()));
            if (command.trimmed().isEmpty())
            {
                DebugTrace::verbose(
                    "tool-registry",
                    QStringLiteral("skip custom cli id=%1 reason=empty-command")
                        .arg(publicId));
                continue;
            }
            const QString cwd = QString::fromStdString(
                config.value("cwd", workspace.toStdString()));
            const QString inputMode = QString::fromStdString(
                config.value("input_mode", std::string("stdin_json")));
            registerTool(std::make_unique<CliCustomTool>(
                publicId,
                description.isEmpty()
                    ? QStringLiteral("Custom CLI tool %1").arg(publicId)
                    : description,
                schema, command,
                jsonStringList(config.value("args", json::array())), cwd,
                jsonEnv(config.value("env", json::object())), inputMode,
                approvalRequired));
            continue;
        }

        if (type == QStringLiteral("mcp"))
        {
            McpTool::ServerConfig serverConfig;
            serverConfig.serverId = publicId;
            serverConfig.command =
                QString::fromStdString(config.value("command", std::string()));
            serverConfig.args =
                jsonStringList(config.value("args", json::array()));
            serverConfig.cwd = QString::fromStdString(
                config.value("cwd", workspace.toStdString()));
            serverConfig.env = jsonEnv(config.value("env", json::object()));
            serverConfig.timeoutMs = config.value("timeout_ms", 30000);
            serverConfig.approvalRequired = approvalRequired;

            QString error;
            auto tools = McpTool::discover(serverConfig, &error);
            if (tools.empty())
            {
                DebugTrace::verbose(
                    "tool-registry",
                    QStringLiteral("skip custom mcp id=%1 reason=%2")
                        .arg(publicId, error));
                continue;
            }
            for (auto &toolPtr : tools)
                registerTool(std::move(toolPtr));
        }
    }
}

Tool *ToolRegistry::find(const QString &id) const
{
    auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : it->second;
}

bool ToolRegistry::exists(const QString &id) const
{
    return find(id) != nullptr;
}

bool ToolRegistry::permissionRequired(const QString &id) const
{
    const Tool *t = find(id);
    return t ? t->permissionRequired() : true;
}

void ToolRegistry::setScheduledTaskManager(ScheduledTaskManager *manager)
{
    m_scheduledTasks = manager;
    if (!m_scheduledTasks || m_scheduledToolsRegistered || !m_builtInEnabled)
        return;
    auto add = [this](std::unique_ptr<Tool> tool)
    {
        m_byId[tool->id()] = tool.get();
        m_tools.push_back(std::move(tool));
    };
    add(std::make_unique<CreateScheduledTaskTool>(m_scheduledTasks));
    add(std::make_unique<SearchScheduledTasksTool>(m_scheduledTasks));
    add(std::make_unique<DeleteScheduledTaskTool>(m_scheduledTasks));
    m_scheduledToolsRegistered = true;
}

QJsonArray ToolRegistry::openaiToolsArray(bool schedulingAvailable) const
{
    // Each entry:
    // {"type":"function","function":{"name","description","parameters"}}
    QJsonArray out;
    for (const auto &t : m_tools)
    {
        if (!schedulingAvailable &&
            t->id().contains(QStringLiteral("scheduled_task")))
            continue;
        const std::string schemaStr = t->schema().dump();
        const QJsonObject params =
            QJsonDocument::fromJson(QString::fromStdString(schemaStr).toUtf8())
                .object();
        QJsonObject fn;
        fn.insert("name", t->id());
        fn.insert("description", t->description());
        fn.insert("parameters", params);
        QJsonObject entry;
        entry.insert("type", QStringLiteral("function"));
        entry.insert("function", fn);
        out.append(entry);
    }
    return out;
}

void ToolRegistry::dispatch(const QString &ownerId, const QString &toolCallId,
                            const QString &name, const QJsonObject &args,
                            const QString &workdir)
{
    Tool *tool = find(name);
    if (!tool)
    {
        emit toolFinished(ownerId, toolCallId, name,
                          QStringLiteral("Error: unknown tool '%1'").arg(name));
        return;
    }

    // QJsonObject → nlohmann::json via byte round-trip. Cheap for tool-sized
    // args.
    const QByteArray argsBytes =
        QJsonDocument(args).toJson(QJsonDocument::Compact);
    json argsJson;
    try
    {
        argsJson = json::parse(argsBytes.constData());
    }
    catch (const std::exception &e)
    {
        emit toolFinished(ownerId, toolCallId, name,
                          QStringLiteral("Error: invalid args JSON: %1")
                              .arg(QString::fromUtf8(e.what())));
        return;
    }
    argsJson["__conversation_id"] = ownerId.toStdString();
    if (!workdir.trimmed().isEmpty())
        argsJson["__workdir"] = workdir.toStdString();

    // Run on a detached worker thread; marshal the result back to the UI
    // thread via a queued invokeMethod before emitting. The registry is a
    // context property living for the app's lifetime, so capturing `this` and
    // the tool pointer is safe. (Parallel dispatch is supported — each call
    // gets its own thread.)
    Tool *toolPtr = tool;
    std::thread(
        [this, ownerId, toolCallId, name, toolPtr,
         argsJson = std::move(argsJson)]()
        {
            const QString result = toolPtr->execute(argsJson);
            QMetaObject::invokeMethod(
                this, [this, ownerId, toolCallId, name, result]
                { emit toolFinished(ownerId, toolCallId, name, result); },
                Qt::QueuedConnection);
        })
        .detach();
}
