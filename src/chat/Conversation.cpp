#include "Conversation.h"

#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QVariant>

#include <nlohmann/json.hpp>

#include "api/OpenAIClient.h"
#include "chat/CompactSupport.h"
#include "core/DebugTrace.h"
#include "core/Settings.h"
#include "tools/ToolRegistry.h"

using json = nlohmann::json;

namespace
{
bool isExecFamilyTool(const QString &toolName)
{
    const QString normalized = toolName.trimmed().toLower();
    return normalized == QStringLiteral("exec") ||
           normalized == QStringLiteral("shell_exec") ||
           normalized == QStringLiteral("root_exec");
}

bool isManExecAllowedInPlanMode(const QString &toolName,
                                const QString &argsText)
{
    if (!isExecFamilyTool(toolName))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(argsText.toUtf8());
    if (!doc.isObject())
        return false;
    const QString command =
        doc.object().value(QStringLiteral("command")).toString().trimmed();
    return command.startsWith(QStringLiteral("man"), Qt::CaseInsensitive);
}

bool isToolBlockedInPlanMode(const QString &toolName, const QString &argsText)
{
    static const QStringList blocked = {
        QStringLiteral("edit"),      QStringLiteral("overwrite"),
        QStringLiteral("exec"),      QStringLiteral("shell_exec"),
        QStringLiteral("root_exec"), QStringLiteral("broadcast"),
    };
    if (!blocked.contains(toolName.trimmed(), Qt::CaseInsensitive))
        return false;
    return !isManExecAllowedInPlanMode(toolName, argsText);
}
} // namespace

Conversation::Conversation(Modes::Mode mode, QString indexMd, QString skillsMd,
                           QObject *parent)
    : QAbstractListModel(parent),
      m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)),
      m_title(QStringLiteral("New conversation")), m_mode(mode),
      m_indexMd(std::move(indexMd)), m_skillsMd(std::move(skillsMd)),
      m_created(QDateTime::currentDateTime()), m_updated(m_created)
{
}

int Conversation::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> Conversation::roleNames() const
{
    return {
        {KindRole, "kind"},
        {TextRole, "text"},
        {ToolCallIdRole, "toolCallId"},
        {ToolNameRole, "toolName"},
        {ArgsTextRole, "argsText"},
        {StatusRole, "status"},
        {ResultRole, "result"},
        {NeedsApprovalRole, "needsApproval"},
        {ImagePathRole, "imagePath"},
        {ImagePathsRole, "imagePaths"},
    };
}

QVariant Conversation::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= int(m_items.size()))
        return {};
    const Item &it = m_items.at(index.row());
    switch (role)
    {
    case KindRole:
        return it.kind;
    case TextRole:
        return it.text;
    case ToolCallIdRole:
        return it.toolCallId;
    case ToolNameRole:
        return it.toolName;
    case ArgsTextRole:
        return it.argsText;
    case StatusRole:
        return it.status;
    case ResultRole:
        return it.result;
    case NeedsApprovalRole:
        return it.needsApproval;
    case ImagePathRole:
        return it.imagePaths.isEmpty() ? QString() : it.imagePaths.first();
    case ImagePathsRole:
        return it.imagePaths;
    }
    return {};
}

void Conversation::setTitle(const QString &t)
{
    if (m_title == t)
        return;
    m_title = t;
    emit titleChanged();
    touch();
}

void Conversation::setStreaming(bool on)
{
    if (m_streaming == on)
        return;
    m_streaming = on;
    emit streamingChanged();
}

void Conversation::touch()
{
    m_updated = QDateTime::currentDateTime();
    emit updatedChanged();
}

void Conversation::appendRow(const Item &it)
{
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("appendRow id=%1 kind=%2 textLen=%3 argsLen=%4 "
                       "resultLen=%5 images=%6 nextRow=%7")
            .arg(m_id, it.kind)
            .arg(it.text.size())
            .arg(it.argsText.size())
            .arg(it.result.size())
            .arg(it.imagePaths.size())
            .arg(m_items.size()));
    beginInsertRows(QModelIndex(), int(m_items.size()), int(m_items.size()));
    m_items.append(it);
    endInsertRows();
}

void Conversation::updateRow(int i, const Item &it)
{
    if (i < 0 || i >= int(m_items.size()))
        return;
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("updateRow id=%1 row=%2 kind=%3 status=%4 "
                       "textLen=%5 argsLen=%6 resultLen=%7")
            .arg(m_id)
            .arg(i)
            .arg(it.kind, it.status)
            .arg(it.text.size())
            .arg(it.argsText.size())
            .arg(it.result.size()));
    m_items[i] = it;
    emit dataChanged(index(i), index(i));
}

int Conversation::indexOfTool(const QString &toolCallId) const
{
    for (int i = 0; i < int(m_items.size()); ++i)
        if (m_items[i].kind == "tool" && m_items[i].toolCallId == toolCallId)
            return i;
    return -1;
}

void Conversation::appendUser(const QString &text)
{
    appendUserWithImages(text, {});
}

void Conversation::appendUserWithImages(const QString &text,
                                        const QStringList &imagePaths)
{
    appendRow({"user", text, "", "", "", "", "", true, imagePaths});
    // Auto-title from the first user message.
    if (m_title == QStringLiteral("New conversation") &&
        !text.trimmed().isEmpty())
    {
        QString t = text;
        t.truncate(40);
        setTitle(t);
    }
    touch();
}

void Conversation::appendAssistant()
{
    appendRow({"assistant", "", "", "", "", "", "", true});
    setStreaming(true);
    touch();
}

void Conversation::appendAssistantText(const QString &text)
{
    appendRow({"assistant", text, "", "", "", "", "", true});
    setStreaming(false);
    touch();
}

void Conversation::appendCompactBoundary(const QString &text)
{
    appendRow({"compact", text, "", "", "", "", "", false});
    touch();
}

void Conversation::appendAssistantDelta(const QString &delta)
{
    if (m_items.isEmpty() || m_items.last().kind != "assistant")
    {
        appendAssistant();
    }
    Item &it = m_items.last();
    it.text.append(delta);
    const int i = int(m_items.size()) - 1;
    emit dataChanged(index(i), index(i));
    touch();
}

void Conversation::appendToolCall(const QString &toolCallId,
                                  const QString &name)
{
    // Dedupe by toolCallId: some OpenAI-compatible endpoints re-send the id on
    // every arguments fragment, and StreamAssembler guards onToolCallStart —
    // but be defensive here too so a duplicate signal never spawns a second
    // card. OpenAI guarantees unique ids per call within one turn.
    if (indexOfTool(toolCallId) >= 0)
        return;
    appendRow({"tool", "", toolCallId, name, "", "composing", "", true});
    touch();
}

void Conversation::setToolReady(const QString &toolCallId, const QString &name,
                                const QString &argsText, bool needsApproval)
{
    int i = indexOfTool(toolCallId);
    if (i < 0)
    {
        appendRow({"tool", "", toolCallId, name, argsText, "pending", "",
                   needsApproval});
        touch();
        return;
    }
    Item it = m_items[i];
    it.toolName = name;
    it.argsText = argsText;
    it.status = "pending";
    it.needsApproval = needsApproval;
    updateRow(i, it);
    touch();
}

// (setToolReady handles both the streaming case — composing row exists — and
// the non-streaming case where the row is created on the spot.)

void Conversation::setToolRunning(const QString &toolCallId)
{
    int i = indexOfTool(toolCallId);
    if (i < 0)
        return;
    Item it = m_items[i];
    it.status = "running";
    updateRow(i, it);
    touch();
}

void Conversation::setToolDone(const QString &toolCallId, const QString &result)
{
    int i = indexOfTool(toolCallId);
    if (i < 0)
        return;
    Item it = m_items[i];
    const bool ok = !result.startsWith("Error:", Qt::CaseInsensitive);
    it.status = ok ? "done" : "error";
    it.result = result;
    updateRow(i, it);
    touch();
}

void Conversation::setToolDenied(const QString &toolCallId)
{
    int i = indexOfTool(toolCallId);
    if (i < 0)
        return;
    Item it = m_items[i];
    it.status = "denied";
    it.result =
        QStringLiteral("Denied by user: tool execution was not approved.");
    updateRow(i, it);
    touch();
}

void Conversation::setToolError(const QString &toolCallId,
                                const QString &result)
{
    int i = indexOfTool(toolCallId);
    if (i < 0)
        return;
    Item it = m_items[i];
    it.status = "error";
    it.result = result;
    updateRow(i, it);
    touch();
}

QJsonArray Conversation::buildMessages() const
{
    return buildMessagesFromRow(firstLiveRow());
}

bool Conversation::schedulingToolsAvailableForCurrentTurn() const
{
    if (!m_scheduledTaskId.isEmpty() || !m_scheduledInstruction.isEmpty())
        return false;
    return m_mode != Modes::Mode::Coding;
}

int Conversation::firstLiveRow() const
{
    return qBound(0, m_compactUntilRow, int(m_items.size()));
}

QJsonArray Conversation::buildMessagesFromRow(int startRow) const
{
    // Prepend the system prompt.
    QJsonArray out;
    QJsonObject sys;
    sys.insert("role", "system");
    QString systemPrompt = Modes::systemPrompt(m_mode, m_indexMd, m_skillsMd);
    const QDateTime now = QDateTime::currentDateTime();
    systemPrompt += QStringLiteral(
                        "\n\nCurrent local date/time: %1\n"
                        "Current local timezone: %2\n")
                        .arg(now.toString(Qt::ISODate),
                             QString::fromUtf8(now.timeZone().id()));
    if (!m_workdir.trimmed().isEmpty())
    {
        systemPrompt +=
            QStringLiteral("\n\nCurrent conversation working directory: %1\n"
                           "Treat this as the default base directory for "
                           "relative paths and "
                           "shell commands unless the user says otherwise.\n")
                .arg(m_workdir);
    }
    if (m_planMode)
    {
        systemPrompt += QStringLiteral(
            "\n\n--- Plan Mode ---\n"
            "You are in plan mode. Do not jump into implementation. First "
            "explore "
            "the problem, gather context, "
            "identify constraints, and propose a clear implementation plan. "
            "Ask "
            "targeted clarifying questions when needed. "
            "Prefer planning, decomposition, and tradeoff analysis over making "
            "changes immediately.\n"
            "When you provide a plan, make it concrete and execution-ready. Do "
            "not "
            "implement until the user explicitly approves leaving plan "
            "mode.\n");
        if (m_planAwaitingApproval)
        {
            systemPrompt +=
                QStringLiteral("\nThe current plan has been submitted "
                               "for approval. Do not implement. "
                               "Wait for approval, or revise the plan "
                               "only if the user asks for changes.\n");
        }
        if (!m_planText.trimmed().isEmpty())
            systemPrompt += QStringLiteral("\nCurrent saved plan:\n%1\n")
                                .arg(m_planText.trimmed());
    }
    sys.insert("content", systemPrompt);
    out.append(sys);
    if (!m_scheduledInstruction.isEmpty())
    {
        QJsonObject scheduled;
        scheduled.insert("role", "system");
        scheduled.insert(
            "content",
            QStringLiteral(
                "You are now executing an already-created autonomous "
                "scheduled task. The task has already been created and has "
                "already fired. This turn is the execution phase, not the "
                "creation phase. create_scheduled_task is forbidden in this "
                "turn unless the task instruction explicitly requires "
                "scheduled-task management. Do not create, reschedule, "
                "search, or delete any scheduled task unless the task "
                "instruction explicitly requires scheduled-task "
                "management. Do not mention a user message or ask for "
                "approval. This scheduled execution turn has full tool "
                "permissions by default, regardless of whether bypass "
                "permissions is enabled in settings. Complete the task, use "
                "tools when needed, then report only the concrete execution "
                "result for this run. Do not say the task was just set, do "
                "not say it will trigger later, do not restate the schedule, "
                "and do not acknowledge task creation success. Treat the "
                "schedule as already elapsed and the trigger as already "
                "happened. When carrying out this scheduled task, use the "
                "current local date/time from the system prompt as the "
                "authoritative current time for this run, and explicitly take "
                "that current time into account if the task depends on timing "
                "or schedule context.\n\nTask being executed right now:\n%1")
                .arg(m_scheduledInstruction));
        out.append(scheduled);

        QJsonObject scheduledAnchor;
        scheduledAnchor.insert("role", "user");
        scheduledAnchor.insert(
            "content",
            QStringLiteral(
                "This turn is a scheduled-task execution turn. The scheduled "
                "task was created earlier and has already triggered. This is "
                "not a task-creation request, not a scheduling confirmation, "
                "and not a future reminder setup. There is no new user reply "
                "to answer right now. Do not say the task was just set, do "
                "not say it will trigger later, do not repeat the schedule, "
                "and do not acknowledge creation success. Treat the trigger "
                "as already happened, execute the already-triggered task "
                "described by the system prompt, and reply only with the "
                "concrete execution result for this run."));        out.append(scheduledAnchor);
    }
    if (!m_compactSummary.trimmed().isEmpty())
    {
        QJsonObject compactMessage;
        compactMessage.insert("role", "user");
        compactMessage.insert("content", m_compactSummary);
        out.append(compactMessage);
    }

    int n = int(m_items.size());
    int i = qBound(0, startRow, n);
    while (i < n)
    {
        const Item &row = m_items[i];
        if (row.kind == "compact")
        {
            ++i;
            continue;
        }
        if (row.kind == "user")
        {
            QJsonObject m;
            m.insert("role", "user");
            if (row.imagePaths.isEmpty())
            {
                m.insert("content", row.text);
            }
            else
            {
                QJsonArray content;
                if (!row.text.trimmed().isEmpty())
                {
                    QJsonObject textPart;
                    textPart.insert("type", "text");
                    textPart.insert("text", row.text);
                    content.append(textPart);
                }

                for (const QString &imagePath : row.imagePaths)
                {
                    QFile imageFile(imagePath);
                    if (!imageFile.open(QIODevice::ReadOnly))
                        continue;

                    const QByteArray bytes = imageFile.readAll();
                    const QString mime =
                        QMimeDatabase().mimeTypeForFile(imagePath).name();
                    const QString dataUrl =
                        QStringLiteral("data:%1;base64,%2")
                            .arg(
                                mime.isEmpty()
                                    ? QStringLiteral("application/octet-stream")
                                    : mime,
                                QString::fromLatin1(bytes.toBase64()));
                    QJsonObject imagePart;
                    imagePart.insert("type", "image_url");
                    QJsonObject imageUrl;
                    imageUrl.insert("url", dataUrl);
                    imagePart.insert("image_url", imageUrl);
                    content.append(imagePart);
                }

                if (content.isEmpty())
                {
                    m.insert("content", row.text);
                }
                else
                {
                    m.insert("content", content);
                }
            }
            out.append(m);
            ++i;
        }
        else if (row.kind == "assistant")
        {
            // Collect following terminal tool rows into tool_calls. Pending,
            // and running rows are UI/runtime state and must not be serialized
            // back into OpenAI history. Terminal failures are model-visible:
            // they let the assistant inspect the error and recover.
            int j = i + 1;
            QJsonArray toolCalls;
            QVector<int> serializableToolRows;
            while (j < n && m_items[j].kind == "tool")
            {
                const Item &t = m_items[j];
                const bool terminalForModel =
                    (t.status == "done" || t.status == "denied" ||
                     t.status == "error") &&
                    !t.result.isEmpty();
                if (terminalForModel)
                {
                    QJsonObject tc;
                    tc.insert("id", t.toolCallId);
                    tc.insert("type", QStringLiteral("function"));
                    QJsonObject fn;
                    fn.insert("name", t.toolName);
                    fn.insert("arguments", t.argsText.isEmpty()
                                               ? QStringLiteral("{}")
                                               : t.argsText);
                    tc.insert("function", fn);
                    toolCalls.append(tc);
                    serializableToolRows.append(j);
                }
                ++j;
            }
            // Skip a trailing empty assistant placeholder (no text, no
            // model-visible tool calls). It's a UI artifact — a row reserved
            // for streaming deltas — and sending it as an empty-content
            // assistant message triggers 400s on stricter OpenAI-compatible
            // endpoints.
            if (row.text.isEmpty() && toolCalls.isEmpty())
            {
                i = j;
                continue;
            }
            QJsonObject m;
            m.insert("role", "assistant");
            m.insert("content", row.text);
            if (!toolCalls.isEmpty())
                m.insert("tool_calls", toolCalls);
            out.append(m);
            // Emit each terminal tool result in the same order as tool_calls.
            for (int k : serializableToolRows)
            {
                const Item &t = m_items[k];
                QJsonObject tr;
                tr.insert("role", "tool");
                tr.insert("tool_call_id", t.toolCallId);
                tr.insert("content", t.result);
                out.append(tr);
            }
            i = j;
        }
        else
        {
            // Orphan tool row (no preceding assistant) — only terminal
            // model-visible results are serialized; transient UI rows are
            // skipped.
            const bool terminalForModel =
                (row.status == "done" || row.status == "denied" ||
                 row.status == "error") &&
                !row.result.isEmpty();
            if (terminalForModel)
            {
                QJsonObject tr;
                tr.insert("role", "tool");
                tr.insert("tool_call_id", row.toolCallId);
                tr.insert("content", row.result);
                out.append(tr);
            }
            ++i;
        }
    }
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("buildMessages id=%1 rows=%2 messages=%3")
            .arg(m_id)
            .arg(m_items.size())
            .arg(out.size()));
    return out;
}

QJsonObject Conversation::toJson() const
{
    QJsonObject o;
    o.insert("id", m_id);
    o.insert("title", m_title);
    o.insert("mode", QString::fromLatin1(Modes::id(m_mode)));
    o.insert("created", m_created.toMSecsSinceEpoch());
    o.insert("updated", m_updated.toMSecsSinceEpoch());
    o.insert("compactSummary", m_compactSummary);
    o.insert("compactUntilRow", m_compactUntilRow);
    o.insert("planMode", m_planMode);
    o.insert("planText", m_planText);
    o.insert("planAwaitingApproval", m_planAwaitingApproval);
    o.insert("workdir", m_workdir);
    QJsonArray items;
    for (const Item &it : m_items)
    {
        QJsonObject r;
        r.insert("kind", it.kind);
        r.insert("text", it.text);
        r.insert("toolCallId", it.toolCallId);
        r.insert("toolName", it.toolName);
        r.insert("argsText", it.argsText);
        r.insert("status", it.status);
        r.insert("result", it.result);
        r.insert("needsApproval", it.needsApproval);
        r.insert("imagePath",
                 it.imagePaths.isEmpty() ? QString() : it.imagePaths.first());
        r.insert("imagePaths", QJsonArray::fromStringList(it.imagePaths));
        items.append(r);
    }
    o.insert("items", items);
    return o;
}

Conversation *Conversation::fromJson(const QJsonObject &obj, QString indexMd,
                                     QString skillsMd, QObject *parent)
{
    const QString modeStr = obj.value("mode").toString("agent");
    Conversation *c =
        new Conversation(Modes::fromId(modeStr), std::move(indexMd),
                         std::move(skillsMd), parent);
    c->m_id = obj.value("id").toString(c->m_id);
    c->m_title = obj.value("title").toString(c->m_title);
    c->m_created = QDateTime::fromMSecsSinceEpoch(
        obj.value("created").toVariant().toLongLong());
    c->m_updated = QDateTime::fromMSecsSinceEpoch(
        obj.value("updated").toVariant().toLongLong());
    c->m_compactSummary = obj.value("compactSummary").toString();
    c->m_compactUntilRow = qMax(0, obj.value("compactUntilRow").toInt(0));
    c->m_planMode = obj.value("planMode").toBool(false);
    c->m_planText = obj.value("planText").toString();
    c->m_planAwaitingApproval = obj.value("planAwaitingApproval").toBool(false);
    c->m_workdir = obj.value("workdir").toString();
    const QJsonArray items = obj.value("items").toArray();
    for (const QJsonValue &v : items)
    {
        const QJsonObject r = v.toObject();
        Item it;
        it.kind = r.value("kind").toString();
        it.text = r.value("text").toString();
        it.toolCallId = r.value("toolCallId").toString();
        it.toolName = r.value("toolName").toString();
        it.argsText = r.value("argsText").toString();
        // On reload, tool items that were composing/running are stale — mark
        // done-ish.
        it.status = r.value("status").toString();
        if (it.status == "composing" || it.status == "pending" ||
            it.status == "running")
            it.status = "done";
        it.result = r.value("result").toString();
        it.needsApproval = r.value("needsApproval").toBool(true);
        if (r.contains("imagePaths") && r.value("imagePaths").isArray())
        {
            const QJsonArray paths = r.value("imagePaths").toArray();
            for (const QJsonValue &pathValue : paths)
                it.imagePaths.append(pathValue.toString());
        }
        else
        {
            const QString legacyPath = r.value("imagePath").toString();
            if (!legacyPath.isEmpty())
                it.imagePaths.append(legacyPath);
        }
        c->m_items.append(it);
    }
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("fromJson id=%1 title=%2 items=%3 mode=%4")
            .arg(c->m_id, c->m_title)
            .arg(c->m_items.size())
            .arg(modeStr));
    return c;
}

// --- per-conversation client + turn lifecycle ---

void Conversation::setContext(Settings *settings, ToolRegistry *registry)
{
    m_settings = settings;
    m_registry = registry;
}

void Conversation::setDefaultWorkdir(QString workdir)
{
    const QString cleaned = QDir::cleanPath(workdir);
    m_defaultWorkdir = cleaned;
    if (m_workdir.trimmed().isEmpty() && !cleaned.isEmpty())
    {
        m_workdir = cleaned;
        emit workdirChanged();
    }
}

void Conversation::ensureClient()
{
    if (m_client)
        return;
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("ensureClient id=%1 create client").arg(m_id));
    m_client = new OpenAIClient(this);
    m_client->setBaseUrl(m_settings->apiBaseUrl());
    m_client->setApiKey(m_settings->apiKey());
    m_client->setModel(m_settings->model());
    m_client->setStreaming(m_settings->streaming());
    connect(m_client, &OpenAIClient::contentDelta, this,
            &Conversation::onClientDelta);
    connect(m_client, &OpenAIClient::toolCallComposing, this,
            &Conversation::onClientComposing);
    connect(m_client, &OpenAIClient::toolCallName, this,
            &Conversation::onClientToolName);
    connect(m_client, &OpenAIClient::toolCallReady, this,
            &Conversation::onClientReady);
    connect(m_client, &OpenAIClient::finished, this,
            &Conversation::onClientFinished);
    connect(m_client, &OpenAIClient::error, this, &Conversation::onClientError);
    // Tool results come from the shared ToolRegistry; filter by toolCallId so
    // only the conversation that dispatched the call processes the result.
    connect(m_registry, &ToolRegistry::toolFinished, this,
            &Conversation::onToolFinished);
}

void Conversation::send(const QString &text) { sendWithImages(text, {}); }

void Conversation::sendWithImages(const QString &text,
                                  const QStringList &imagePaths)
{
    if ((text.trimmed().isEmpty() && imagePaths.isEmpty()) || !m_settings ||
        !m_registry)
        return;
    DebugTrace::verbose(
        "conversation",
        QStringLiteral(
            "sendWithImages id=%1 textLen=%2 images=%3 model=%4 streaming=%5")
            .arg(m_id)
            .arg(text.size())
            .arg(imagePaths.size())
            .arg(m_settings ? m_settings->model() : QString())
            .arg(m_settings && m_settings->streaming()));
    appendUserWithImages(text, imagePaths);
    maybeAutoCompact({}, {}, true);
    appendAssistant();
    ensureClient();
    // Re-sync config each turn so settings changes (key/model/endpoint) are
    // picked up without restarting.
    m_client->setBaseUrl(m_settings->apiBaseUrl());
    m_client->setApiKey(m_settings->apiKey());
    m_client->setModel(m_settings->model());
    m_client->setStreaming(m_settings->streaming());
    m_compactRetryAttempted = false;
    setError(QString());
    m_client->send(buildMessages(),
                   m_registry->openaiToolsArray(
                       schedulingToolsAvailableForCurrentTurn()));
}

void Conversation::runScheduledTask(const QString &taskId,
                                    const QString &instruction)
{
    if (taskId.isEmpty() || instruction.trimmed().isEmpty() || m_streaming ||
        !m_settings || !m_registry)
        return;
    m_scheduledTaskId = taskId;
    m_scheduledInstruction = instruction.trimmed();
    maybeAutoCompact();
    appendAssistant();
    ensureClient();
    m_client->setBaseUrl(m_settings->apiBaseUrl());
    m_client->setApiKey(m_settings->apiKey());
    m_client->setModel(m_settings->model());
    m_client->setStreaming(m_settings->streaming());
    setError(QString());
    m_client->send(buildMessages(),
                   m_registry->openaiToolsArray(
                       schedulingToolsAvailableForCurrentTurn()));
}

QString Conversation::importImage(const QString &sourcePath)
{
    const QFileInfo srcInfo(sourcePath);
    if (!srcInfo.exists() || !srcInfo.isFile())
    {
        setError(QStringLiteral("Image file not found: %1").arg(sourcePath));
        return QString();
    }
    if (m_attachmentsDir.isEmpty())
    {
        setError(QStringLiteral(
            "Conversation attachments directory is not configured."));
        return QString();
    }

    QDir().mkpath(m_attachmentsDir);
    const QString suffix = srcInfo.suffix().isEmpty()
                               ? QStringLiteral("bin")
                               : srcInfo.suffix().toLower();
    const QString destPath =
        QDir(m_attachmentsDir)
            .filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) +
                      QStringLiteral(".") + suffix);

    if (QFile::exists(destPath))
        QFile::remove(destPath);
    if (!QFile::copy(sourcePath, destPath))
    {
        setError(QStringLiteral("Failed to import image: %1").arg(sourcePath));
        return QString();
    }
    DebugTrace::verbose("conversation",
                        QStringLiteral("importImage id=%1 src=%2 dst=%3")
                            .arg(m_id, sourcePath, destPath));
    return destPath;
}

void Conversation::dispatch(const QString &toolCallId, const QString &name,
                            const QString &argsText)
{
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("dispatch id=%1 toolCallId=%2 name=%3 argsLen=%4")
            .arg(m_id, toolCallId, name)
            .arg(argsText.size()));
    if (m_planMode && isToolBlockedInPlanMode(name, argsText))
    {
        const QString message =
            QStringLiteral(
                "Error: tool `%1` is unavailable in plan mode. Stay in "
                "exploration/planning, "
                "or exit plan mode before implementation.")
                .arg(name);
        DebugTrace::verbose(
            "conversation",
            QStringLiteral("dispatch blocked by plan mode id=%1 tool=%2")
                .arg(m_id, name));
        setToolError(toolCallId, message);
        const int i = indexOfTool(toolCallId);
        if (i >= 0)
            maybeContinueAfterToolResolution(i);
        return;
    }
    setToolRunning(toolCallId);
    const QJsonObject argsObj =
        argsText.isEmpty()
            ? QJsonObject()
            : QJsonDocument::fromJson(argsText.toUtf8()).object();
    m_registry->dispatch(m_id, toolCallId, name, argsObj, m_workdir);
}

void Conversation::denyTool(const QString &toolCallId)
{
    setToolDenied(toolCallId);
    const int i = indexOfTool(toolCallId);
    if (i >= 0)
        maybeContinueAfterToolResolution(i);
}

void Conversation::setError(const QString &msg)
{
    if (m_error == msg)
        return;
    DebugTrace::verbose("conversation",
                        QStringLiteral("setError id=%1 len=%2 text=%3")
                            .arg(m_id)
                            .arg(msg.size())
                            .arg(msg));
    m_error = msg;
    emit errorChanged();
}

void Conversation::onClientDelta(const QString &text)
{
    DebugTrace::verbose("conversation",
                        QStringLiteral("onClientDelta id=%1 deltaLen=%2")
                            .arg(m_id)
                            .arg(text.size()));
    appendAssistantDelta(text);
}

void Conversation::onClientComposing(const QString &id, const QString &name)
{
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("onClientComposing id=%1 toolCallId=%2 name=%3")
            .arg(m_id, id, name));
    appendToolCall(id, name);
}

void Conversation::onClientToolName(const QString &id, const QString &name)
{
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("onClientToolName id=%1 toolCallId=%2 name=%3")
            .arg(m_id, id, name));
    // The name may arrive after the composing card was created with an empty
    // name. Prefer patching by toolCallId; if the endpoint still has no id,
    // fall back to the most recent composing card whose name is empty.
    if (!id.isEmpty())
    {
        const int idx = indexOfTool(id);
        if (idx >= 0 && m_items[idx].toolName.isEmpty())
        {
            Item it = m_items[idx];
            it.toolName = name;
            updateRow(idx, it);
            return;
        }
    }
    for (int i = int(m_items.size()) - 1; i >= 0; --i)
    {
        Item &it = m_items[i];
        if (it.kind == "tool" && it.status == "composing" &&
            it.toolName.isEmpty())
        {
            it.toolName = name;
            emit dataChanged(index(i), index(i));
            return;
        }
    }
}

void Conversation::onClientReady(const QString &id, const QString &name,
                                 const QJsonObject &args)
{
    const QString argsStr =
        QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Indented));
    const bool scheduledExecution = !m_scheduledTaskId.isEmpty();
    const bool needs =
        scheduledExecution ? false : m_registry->permissionRequired(name);
    DebugTrace::verbose("conversation",
                        QStringLiteral("onClientReady id=%1 toolCallId=%2 "
                                       "name=%3 needsApproval=%4 scheduled=%5 "
                                       "argsKeys=%6")
                            .arg(m_id, id, name)
                            .arg(needs)
                            .arg(scheduledExecution)
                            .arg(args.keys().join(',')));
    setToolReady(id, name, argsStr, needs);
    // bypass → auto-dispatch. No approval gate.
    if (scheduledExecution || m_settings->bypassPermissions() || !needs)
        dispatch(id, name, argsStr);
}

void Conversation::onClientFinished()
{
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("onClientFinished id=%1 pendingResend=%2")
            .arg(m_id)
            .arg(m_pendingResend));
    setStreaming(false);
    // A tool may have finished before the stream's finished() signal arrived
    // (fast tools). If so, the resend was deferred — do it now that the client
    // is idle.
    if (m_pendingResend)
    {
        m_pendingResend = false;
        // The new request is a streaming turn — flip streaming back on so the
        // UI shows "replying" while the model responds to the tool result.
        setStreaming(true);
        maybeAutoCompact();
        m_client->send(buildMessages(),
                       m_registry->openaiToolsArray(
                           schedulingToolsAvailableForCurrentTurn()));
    }
    if (m_planMode)
    {
        for (int i = int(m_items.size()) - 1; i >= 0; --i)
        {
            if (m_items[i].kind == QStringLiteral("assistant"))
            {
                const QString plan = m_items[i].text.trimmed();
                if (!plan.isEmpty() && plan != m_planText)
                {
                    m_planText = plan;
                    emit planTextChanged();
                    persistPlanFile();
                    touch();
                }
                break;
            }
        }
    }
    if (!m_scheduledTaskId.isEmpty() && !m_pendingResend &&
        !hasUnresolvedToolCalls())
        finishScheduledTask(true);
}

void Conversation::onClientError(const QString &msg)
{
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("onClientError id=%1 len=%2").arg(m_id).arg(msg.size()));
    const QString lower = msg.toLower();
    const bool contextExceeded =
        lower.contains(QStringLiteral("context_length_exceeded")) ||
        lower.contains(QStringLiteral("context window")) ||
        lower.contains(QStringLiteral("input exceeds the context")) ||
        lower.contains(QStringLiteral("prompt too long"));
    if (contextExceeded && !m_compactRetryAttempted && m_settings &&
        m_settings->compact())
    {
        DebugTrace::verbose(
            "compact",
            QStringLiteral("fallback retry id=%1 reason=context-exceeded")
                .arg(m_id));
        m_compactRetryAttempted = true;
        setStreaming(true);
        if (maybeAutoCompact({}, {}, true))
        {
            setError(QString());
            m_client->send(buildMessages(),
                           m_registry->openaiToolsArray(
                               schedulingToolsAvailableForCurrentTurn()));
            return;
        }
        setStreaming(false);
    }
    setStreaming(false);
    setError(msg);
    if (!m_scheduledTaskId.isEmpty())
        finishScheduledTask(false);
}

bool Conversation::toolGroupReadyToContinue(int toolIndex) const
{
    if (toolIndex < 0 || toolIndex >= int(m_items.size()) ||
        m_items[toolIndex].kind != "tool")
        return false;

    int start = toolIndex;
    while (start > 0 && m_items[start - 1].kind == "tool")
        --start;
    if (start == 0 || m_items[start - 1].kind != "assistant")
        return false;

    int end = start;
    while (end < int(m_items.size()) && m_items[end].kind == "tool")
        ++end;

    for (int i = start; i < end; ++i)
    {
        const QString &status = m_items[i].status;
        if (status != "done" && status != "denied" && status != "error")
            return false;
        if (m_items[i].result.isEmpty())
            return false;
    }
    return true;
}

void Conversation::maybeContinueAfterToolResolution(int toolIndex)
{
    if (!toolGroupReadyToContinue(toolIndex))
        return;

    DebugTrace::verbose("conversation",
                        QStringLiteral("maybeContinueAfterToolResolution id=%1 "
                                       "toolIndex=%2 prevStreaming=%3")
                            .arg(m_id)
                            .arg(toolIndex)
                            .arg(m_streaming));
    const bool prevStreaming = m_streaming;
    appendAssistant();
    if (prevStreaming)
    {
        m_pendingResend = true;
    }
    else
    {
        ensureClient();
        maybeAutoCompact();
        m_client->send(buildMessages(),
                       m_registry->openaiToolsArray(
                           schedulingToolsAvailableForCurrentTurn()));
    }
    if (m_planMode)
    {
        for (int i = int(m_items.size()) - 1; i >= 0; --i)
        {
            if (m_items[i].kind == QStringLiteral("assistant"))
            {
                const QString plan = m_items[i].text.trimmed();
                if (!plan.isEmpty() && plan != m_planText)
                {
                    m_planText = plan;
                    emit planTextChanged();
                    persistPlanFile();
                    touch();
                }
                break;
            }
        }
    }
}

void Conversation::onToolFinished(const QString &ownerId, const QString &id,
                                  const QString &, const QString &result)
{
    if (ownerId != m_id)
        return;
    // Only process results for tool calls that belong to THIS conversation.
    const int toolIndex = indexOfTool(id);
    if (toolIndex < 0)
        return;
    DebugTrace::verbose(
        "conversation",
        QStringLiteral("onToolFinished id=%1 toolCallId=%2 resultLen=%3")
            .arg(m_id, id)
            .arg(result.size()));
    setToolDone(id, result);
    maybeContinueAfterToolResolution(toolIndex);
}

bool Conversation::hasUnresolvedToolCalls() const
{
    for (const Item &item : m_items)
    {
        if (item.kind == QStringLiteral("tool") &&
            item.status != QStringLiteral("done") &&
            item.status != QStringLiteral("denied") &&
            item.status != QStringLiteral("error"))
            return true;
    }
    return false;
}

void Conversation::finishScheduledTask(bool success)
{
    const QString id = m_scheduledTaskId;
    if (id.isEmpty())
        return;
    QString summary = m_error;
    if (summary.isEmpty())
    {
        for (int i = int(m_items.size()) - 1; i >= 0; --i)
        {
            if (m_items[i].kind == QStringLiteral("assistant") &&
                !m_items[i].text.trimmed().isEmpty())
            {
                summary = m_items[i].text.trimmed();
                break;
            }
        }
    }
    if (summary.isEmpty())
        summary = success ? QStringLiteral("Task completed.")
                          : QStringLiteral("Task failed.");
    m_scheduledTaskId.clear();
    m_scheduledInstruction.clear();
    emit scheduledTaskFinished(id, success, summary);
}

int Conversation::estimatedRequestTokens(const QString &pendingText,
                                         const QStringList &pendingImages) const
{
    QJsonArray messages = buildMessagesFromRow(firstLiveRow());
    if (!pendingText.trimmed().isEmpty() || !pendingImages.isEmpty())
    {
        QJsonObject pending;
        pending.insert("role", "user");
        if (pendingImages.isEmpty())
        {
            pending.insert("content", pendingText);
        }
        else
        {
            QJsonArray content;
            if (!pendingText.trimmed().isEmpty())
            {
                QJsonObject textPart;
                textPart.insert("type", "text");
                textPart.insert("text", pendingText);
                content.append(textPart);
            }
            for (int i = 0; i < pendingImages.size(); ++i)
            {
                QJsonObject imagePart;
                imagePart.insert("type", "image_url");
                QJsonObject imageUrl;
                imageUrl.insert(
                    "url", QStringLiteral("data:image/*;base64,[omitted]"));
                imagePart.insert("image_url", imageUrl);
                content.append(imagePart);
            }
            pending.insert("content", content);
        }
        messages.append(pending);
    }
    return CompactSupport::roughTokenCount(messages);
}

int Conversation::chooseCompactionCutRow() const
{
    return chooseCompactionCutRow(int(m_items.size()));
}

int Conversation::chooseCompactionCutRow(int maxEndRowExclusive) const
{
    const int start = firstLiveRow();
    const int n = qBound(start, maxEndRowExclusive, int(m_items.size()));
    if (n - start < 8)
        return -1;

    const int usable = CompactSupport::usableContextWindow(
        m_settings ? m_settings->model() : QString());
    const int tailBudget = qMax(2048, int(double(usable) * 0.35));

    int suffixTokens = 0;
    int cut = n;
    for (int i = n - 1; i >= start; --i)
    {
        const Item &it = m_items[i];
        suffixTokens += CompactSupport::roughTokenCount(
            CompactSupport::summarizeMessageForTranscript(it.kind, it.text,
                                                          it.imagePaths) +
            it.argsText + it.result);
        if (suffixTokens > tailBudget)
        {
            cut = i + 1;
            break;
        }
    }

    if (cut <= start || cut >= n)
        return -1;

    while (cut < n && m_items[cut].kind != QStringLiteral("user"))
        ++cut;
    if (cut >= n)
        return -1;
    if (cut - start < 4)
        return -1;
    return cut;
}

QString Conversation::buildCompactionTranscript(int cutRow) const
{
    QStringList blocks;
    if (!m_compactSummary.trimmed().isEmpty())
    {
        blocks << QStringLiteral("[prior_compact_summary]\n%1")
                      .arg(m_compactSummary.trimmed());
    }

    const int start = firstLiveRow();
    const int end = qBound(start, cutRow, int(m_items.size()));
    for (int i = start; i < end; ++i)
    {
        const Item &it = m_items[i];
        if (it.kind == QStringLiteral("tool"))
        {
            QString toolText = QStringLiteral("[tool]\nname: %1\nstatus: %2")
                                   .arg(it.toolName, it.status);
            if (!it.argsText.trimmed().isEmpty())
                toolText +=
                    QStringLiteral("\nargs:\n%1").arg(it.argsText.trimmed());
            if (!it.result.trimmed().isEmpty())
                toolText +=
                    QStringLiteral("\nresult:\n%1").arg(it.result.trimmed());
            blocks << toolText;
            continue;
        }
        blocks << CompactSupport::summarizeMessageForTranscript(
            it.kind, it.text, it.imagePaths);
    }
    return blocks.join(QStringLiteral("\n\n"));
}

QString Conversation::runCompactionRequest(const QString &transcript,
                                           QString *errorOut,
                                           const QString &instructions) const
{
    if (errorOut)
        errorOut->clear();
    if (!m_settings)
        return {};

    OpenAIClient client;
    client.setBaseUrl(m_settings->apiBaseUrl());
    client.setApiKey(m_settings->apiKey());
    client.setModel(m_settings->model());
    client.setStreaming(false);

    QString result;
    QString error;
    bool done = false;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop,
                     [&]
                     {
                         error = QStringLiteral("compact timed out");
                         done = true;
                         loop.quit();
                     });
    QObject::connect(&client, &OpenAIClient::contentDelta, &loop,
                     [&](const QString &text) { result += text; });
    QObject::connect(&client, &OpenAIClient::finished, &loop,
                     [&]
                     {
                         done = true;
                         loop.quit();
                     });
    QObject::connect(&client, &OpenAIClient::error, &loop,
                     [&](const QString &message)
                     {
                         error = message;
                         done = true;
                         loop.quit();
                     });

    QJsonArray messages;
    QJsonObject system;
    system.insert("role", "system");
    system.insert(
        "content",
        QStringLiteral("You are a careful engineering handoff summarizer."));
    messages.append(system);
    QJsonObject user;
    user.insert("role", "user");
    QString prompt = CompactSupport::summarizationPrompt();
    if (!instructions.trimmed().isEmpty())
        prompt += QStringLiteral("\n\nAdditional instructions:\n") +
                  instructions.trimmed();
    prompt +=
        QStringLiteral("\n\n--- Conversation to compact ---\n") + transcript;
    user.insert("content", prompt);
    messages.append(user);

    timer.start(CompactSupport::kCompactionTimeoutMs);
    client.send(messages, {});
    loop.exec();

    if (!done || !error.isEmpty() || result.trimmed().isEmpty())
    {
        if (errorOut)
            *errorOut = error.isEmpty()
                            ? QStringLiteral("compact returned empty summary")
                            : error;
        return {};
    }
    return result.trimmed();
}

bool Conversation::maybeAutoCompact(const QString &pendingText,
                                    const QStringList &pendingImages,
                                    bool reserveLatestUserTurn)
{
    if (!m_settings || !m_settings->compact())
        return true;

    const int estimated = estimatedRequestTokens(pendingText, pendingImages);
    const int usable = CompactSupport::usableContextWindow(m_settings->model());
    const int threshold = qMax(4096, int(double(usable) * 0.80));
    DebugTrace::verbose(
        "compact",
        QStringLiteral("check id=%1 estimated=%2 usable=%3 threshold=%4 "
                       "pendingTextLen=%5 pendingImages=%6")
            .arg(m_id)
            .arg(estimated)
            .arg(usable)
            .arg(threshold)
            .arg(pendingText.size())
            .arg(pendingImages.size()));
    if (estimated < threshold)
        return true;

    int searchEnd = int(m_items.size());
    if (reserveLatestUserTurn)
    {
        for (int i = int(m_items.size()) - 1; i >= firstLiveRow(); --i)
        {
            if (m_items[i].kind == QStringLiteral("user"))
            {
                searchEnd = i;
                break;
            }
        }
    }

    const int cutRow = chooseCompactionCutRow(searchEnd);
    if (cutRow < 0)
    {
        DebugTrace::verbose(
            "compact",
            QStringLiteral("skip id=%1 reason=no-cut-row").arg(m_id));
        return false;
    }

    const QString transcript = CompactSupport::trimToTokenBudget(
        buildCompactionTranscript(cutRow), qMax(4096, usable / 2));
    QString error;
    const QString summary = runCompactionRequest(transcript, &error);
    if (summary.isEmpty())
    {
        DebugTrace::verbose("compact",
                            QStringLiteral("failed id=%1 cutRow=%2 error=%3")
                                .arg(m_id)
                                .arg(cutRow)
                                .arg(error));
        if (!error.isEmpty())
            setError(QStringLiteral("Auto compact failed: %1").arg(error));
        return false;
    }

    m_compactSummary =
        CompactSupport::summaryPrefix() + QStringLiteral("\n") + summary;
    m_compactUntilRow = cutRow;
    appendCompactBoundary(
        QStringLiteral("Context compacted into a handoff summary."));
    DebugTrace::verbose(
        "compact", QStringLiteral("applied id=%1 cutRow=%2 summaryTokens=%3")
                       .arg(m_id)
                       .arg(cutRow)
                       .arg(CompactSupport::roughTokenCount(m_compactSummary)));
    touch();
    return true;
}

QString Conversation::compactNow(const QString &instructions)
{
    if (!m_settings || !m_settings->compact())
        return QStringLiteral("Compact is disabled in Settings.");

    const int cutRow = chooseCompactionCutRow(int(m_items.size()));
    if (cutRow < 0)
        return QStringLiteral(
            "Not enough earlier conversation to compact yet.");

    const int usable = CompactSupport::usableContextWindow(m_settings->model());
    const QString transcript = CompactSupport::trimToTokenBudget(
        buildCompactionTranscript(cutRow), qMax(4096, usable / 2));
    QString error;
    const QString summary =
        runCompactionRequest(transcript, &error, instructions);
    if (summary.isEmpty())
        return error.isEmpty()
                   ? QStringLiteral("Manual compact failed.")
                   : QStringLiteral("Manual compact failed: %1").arg(error);

    m_compactSummary =
        CompactSupport::summaryPrefix() + QStringLiteral("\n") + summary;
    m_compactUntilRow = cutRow;
    appendCompactBoundary(
        QStringLiteral("Context compacted into a handoff summary."));
    touch();
    return QStringLiteral("Conversation compacted. Older history is now "
                          "represented by a handoff summary.");
}

QString Conversation::enterPlanMode(const QString &note)
{
    const bool wasPlan = m_planMode;
    m_planMode = true;
    if (m_planAwaitingApproval)
    {
        m_planAwaitingApproval = false;
        emit planAwaitingApprovalChanged();
    }
    if (!note.trimmed().isEmpty())
    {
        if (!m_planText.trimmed().isEmpty())
            m_planText += QStringLiteral("\n\n");
        m_planText += QStringLiteral("Plan note:\n%1").arg(note.trimmed());
        emit planTextChanged();
    }
    if (!wasPlan)
        emit planModeChanged();
    persistPlanFile();
    touch();
    return note.trimmed().isEmpty()
               ? QStringLiteral(
                     "Enabled plan mode. I will focus on exploration "
                     "and planning before implementation.")
               : QStringLiteral(
                     "Enabled plan mode and saved your planning note.");
}

QString Conversation::submitPlanForApproval()
{
    if (!m_planMode)
        return QStringLiteral("Plan mode is not currently enabled.");
    if (m_planText.trimmed().isEmpty())
        return QStringLiteral("There is no saved plan to submit yet.");
    if (m_planAwaitingApproval)
        return QStringLiteral(
            "The current plan is already waiting for approval.");
    m_planAwaitingApproval = true;
    emit planAwaitingApprovalChanged();
    persistPlanFile();
    touch();
    return QStringLiteral(
        "Submitted the current plan for approval. I will stay "
        "in plan mode until you approve or reject it.");
}

QString Conversation::approvePlan()
{
    if (!m_planMode)
        return QStringLiteral("Plan mode is not currently enabled.");
    if (!m_planAwaitingApproval)
        return QStringLiteral(
            "The current plan is not waiting for approval yet. "
            "Use `/plan ready` first.");
    m_planAwaitingApproval = false;
    m_planMode = false;
    emit planAwaitingApprovalChanged();
    emit planModeChanged();
    touch();
    return QStringLiteral("Approved the current plan and exited plan mode. "
                          "Normal implementation behavior is restored.");
}

QString Conversation::rejectPlan(const QString &note)
{
    if (!m_planMode)
        return QStringLiteral("Plan mode is not currently enabled.");
    if (!m_planAwaitingApproval)
        return QStringLiteral("The current plan is not waiting for approval.");
    m_planAwaitingApproval = false;
    emit planAwaitingApprovalChanged();
    if (!note.trimmed().isEmpty())
    {
        if (!m_planText.trimmed().isEmpty())
            m_planText += QStringLiteral("\n\n");
        m_planText += QStringLiteral("Revision note:\n%1").arg(note.trimmed());
        emit planTextChanged();
    }
    persistPlanFile();
    touch();
    return note.trimmed().isEmpty()
               ? QStringLiteral(
                     "Rejected the current plan. I will stay in plan "
                     "mode and revise it.")
               : QStringLiteral(
                     "Rejected the current plan and saved your revision note.");
}

QString Conversation::exitPlanMode()
{
    if (!m_planMode)
        return QStringLiteral("Plan mode is not currently enabled.");
    const bool wasAwaitingApproval = m_planAwaitingApproval;
    m_planAwaitingApproval = false;
    m_planMode = false;
    if (wasAwaitingApproval)
        emit planAwaitingApprovalChanged();
    emit planModeChanged();
    touch();
    return wasAwaitingApproval
               ? QStringLiteral("Exited plan mode and discarded the pending "
                                "approval state. "
                                "Normal implementation behavior is restored.")
               : QStringLiteral("Exited plan mode. Normal implementation "
                                "behavior is restored.");
}

QString Conversation::showPlan() const
{
    if (!m_planMode && m_planText.trimmed().isEmpty())
        return QStringLiteral("No active plan.");
    if (m_planText.trimmed().isEmpty())
        return QStringLiteral(
            "Plan mode is enabled, but no plan has been written yet.");
    return m_planText;
}

QString Conversation::clearPlan()
{
    if (m_planText.isEmpty())
        return QStringLiteral("Plan is already empty.");
    m_planText.clear();
    if (m_planAwaitingApproval)
    {
        m_planAwaitingApproval = false;
        emit planAwaitingApprovalChanged();
    }
    emit planTextChanged();
    if (!m_planFilePath.isEmpty())
        QFile::remove(m_planFilePath);
    touch();
    return QStringLiteral("Cleared the saved plan.");
}

QString Conversation::openPlan()
{
    if (m_planFilePath.isEmpty())
        return QStringLiteral("Plan file path is not configured.");

    QDir().mkpath(QFileInfo(m_planFilePath).absolutePath());
    if (!QFile::exists(m_planFilePath))
    {
        QFile file(m_planFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                      QIODevice::Text))
        {
            QTextStream stream(&file);
            stream << (m_planText.trimmed().isEmpty()
                           ? QStringLiteral("# Plan\n\n")
                           : m_planText);
        }
    }

    const bool ok =
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_planFilePath));
    return ok ? QStringLiteral("Opened plan file: %1").arg(m_planFilePath)
              : QStringLiteral("Failed to open plan file: %1")
                    .arg(m_planFilePath);
}

void Conversation::persistPlanFile() const
{
    if (m_planFilePath.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_planFilePath).absolutePath());
    QFile file(m_planFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                   QIODevice::Text))
        return;
    QTextStream stream(&file);
    if (m_planText.trimmed().isEmpty())
    {
        stream << QStringLiteral("# Plan\n\n");
    }
    else
    {
        stream << m_planText.trimmed();
        if (m_planAwaitingApproval)
            stream << QStringLiteral("\n\n[Awaiting approval]");
    }
}

QString Conversation::showWorkdir() const
{
    if (m_workdir.trimmed().isEmpty())
        return QStringLiteral(
            "No working directory is configured for this conversation.");
    if (!m_defaultWorkdir.isEmpty() &&
        QDir::cleanPath(m_workdir) == QDir::cleanPath(m_defaultWorkdir))
        return QStringLiteral("Current workdir: %1 (default workspace)")
            .arg(m_workdir);
    return QStringLiteral("Current workdir: %1").arg(m_workdir);
}

QString Conversation::setWorkdir(const QString &path)
{
    const QString resolved = resolveRequestedWorkdir(path);
    if (resolved.isEmpty())
        return QStringLiteral(
            "Error: workdir must point to an existing directory.");
    if (QDir::cleanPath(resolved) == QDir::cleanPath(m_workdir))
        return QStringLiteral("Workdir is already %1").arg(m_workdir);
    m_workdir = resolved;
    emit workdirChanged();
    touch();
    return QStringLiteral("Set conversation workdir to %1").arg(m_workdir);
}

QString Conversation::resetWorkdir()
{
    if (m_defaultWorkdir.trimmed().isEmpty())
        return QStringLiteral(
            "Error: the default workspace directory is not configured.");
    if (QDir::cleanPath(m_workdir) == QDir::cleanPath(m_defaultWorkdir))
        return QStringLiteral("Workdir is already the default workspace: %1")
            .arg(m_defaultWorkdir);
    m_workdir = m_defaultWorkdir;
    emit workdirChanged();
    touch();
    return QStringLiteral("Reset conversation workdir to %1").arg(m_workdir);
}

QString Conversation::resolveRequestedWorkdir(const QString &path) const
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return QString();

    QString base = m_workdir.trimmed();
    if (base.isEmpty())
        base = m_defaultWorkdir.trimmed();

    QString candidate = trimmed;
    if (!QDir::isAbsolutePath(candidate))
    {
        if (base.isEmpty())
            return QString();
        candidate = QDir(base).filePath(candidate);
    }

    const QFileInfo info(candidate);
    if (!info.exists() || !info.isDir())
        return QString();

    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath())
                               : QDir::cleanPath(canonical);
}
