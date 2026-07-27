#include "WriteMemoryTool.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QUuid>

#include "MemoryToolUtils.h"

using json = nlohmann::json;

WriteMemoryTool::WriteMemoryTool(QString memoriesDir)
    : m_memoriesDir(std::move(memoriesDir))
{
}

QString WriteMemoryTool::description() const
{
    return QStringLiteral(
        "Persist a memory snippet for the current conversation "
        "or globally for future conversations.");
}

json WriteMemoryTool::schema() const
{
    return {{"type", "object"},
            {"properties",
             {{"content",
               {{"type", "string"},
                {"description", "The memory content to save."}}},
              {"title",
               {{"type", "string"},
                {"description", "Optional short title for the memory."}}},
              {"key",
               {{"type", "string"},
                {"description",
                 "Optional stable key/filename slug. Reuses the same "
                 "file if provided."}}},
              {"scope",
               {{"type", "string"},
                {"enum", {"conversation", "global"}},
                {"description", "Storage scope. Defaults to conversation."}}},
              {"tags",
               {{"type", "array"},
                {"items", {{"type", "string"}}},
                {"description", "Optional tags to help later recall."}}}}},
            {"required", {"content"}}};
}

QString WriteMemoryTool::execute(const json &args)
{
    const QString content =
        QString::fromStdString(args.value("content", std::string())).trimmed();
    const QString title =
        QString::fromStdString(args.value("title", std::string())).trimmed();
    const QString key = MemoryToolUtils::sanitizeKey(
        QString::fromStdString(args.value("key", std::string())));
    const QString scope = MemoryToolUtils::normalizeScope(
        QString::fromStdString(args.value("scope", std::string())));
    const QString conversationId =
        QString::fromStdString(args.value("__conversation_id", std::string()))
            .trimmed();

    QStringList tags;
    if (args.contains("tags") && args["tags"].is_array())
    {
        for (const auto &entry : args["tags"])
        {
            if (entry.is_string())
                tags.append(
                    QString::fromStdString(entry.get<std::string>()).trimmed());
        }
        tags.removeAll(QString());
    }

    if (content.isEmpty())
        return QStringLiteral(
            "Error: write_memory requires a non-empty `content`.");
    if (m_memoriesDir.isEmpty())
        return QStringLiteral("Error: memories directory is not configured.");

    const QString scopeDir =
        MemoryToolUtils::resolveScopeDir(m_memoriesDir, scope, conversationId);
    if (scopeDir.isEmpty())
    {
        return QStringLiteral("Error: conversation-scoped memory requires an "
                              "active conversation id.");
    }

    QDir().mkpath(scopeDir);
    QString baseName = key;
    if (baseName.isEmpty())
        baseName = MemoryToolUtils::sanitizeKey(
            !title.isEmpty() ? title : content.left(48));
    if (baseName.isEmpty())
        baseName = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QString filePath =
        QDir(scopeDir).filePath(baseName + QStringLiteral(".md"));
    const QString body = MemoryToolUtils::buildMemoryDocument(
        title, content, tags, scope, conversationId);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QStringLiteral(
                   "Error: failed to open memory file for writing: %1")
            .arg(filePath);

    QTextStream out(&file);
    out << body;
    out.flush();
    if (!file.commit())
        return QStringLiteral("Error: failed to commit memory file: %1")
            .arg(filePath);

    return QStringLiteral("Memory saved.\nscope: %1\npath: %2")
        .arg(scope, QFileInfo(filePath).filePath());
}
