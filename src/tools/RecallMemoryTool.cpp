#include "RecallMemoryTool.h"

#include <QFileInfo>

#include "MemoryToolUtils.h"

using json = nlohmann::json;

RecallMemoryTool::RecallMemoryTool(QString memoriesDir)
    : m_memoriesDir(std::move(memoriesDir))
{
}

QString RecallMemoryTool::description() const
{
    return QStringLiteral("Search saved memories on demand with exact or fuzzy "
                          "matching. Memories "
                          "are not preloaded into the prompt.");
}

json RecallMemoryTool::schema() const
{
    return {{"type", "object"},
            {"properties",
             {{"query",
               {{"type", "string"},
                {"description", "Search term to look up in the memories "
                                "directory. Fuzzy matches are allowed."}}},
              {"limit",
               {{"type", "integer"},
                {"minimum", 1},
                {"maximum", 20},
                {"description", "Maximum number of matches to return."}}},
              {"file_pattern",
               {{"type", "string"},
                {"description", "Optional substring filter for file names."}}},
              {"scope",
               {{"type", "string"},
                {"enum", {"conversation", "global"}},
                {"description",
                 "Which memory scope to search. Defaults to conversation."}}}}},
            {"required", {"query"}}};
}

QString RecallMemoryTool::execute(const json &args)
{
    const QString query =
        QString::fromStdString(args.value("query", std::string())).trimmed();
    const QString filePattern =
        QString::fromStdString(args.value("file_pattern", std::string()))
            .trimmed();
    const QString scope = MemoryToolUtils::normalizeScope(
        QString::fromStdString(args.value("scope", std::string())));
    const QString conversationId =
        QString::fromStdString(args.value("__conversation_id", std::string()))
            .trimmed();
    const int limit = qBound(1, args.value("limit", 5), 20);

    if (query.isEmpty())
        return QStringLiteral(
            "Error: recall_memory requires a non-empty `query`.");
    if (m_memoriesDir.isEmpty())
        return QStringLiteral("Error: memories directory is not configured.");
    if (scope == QStringLiteral("conversation") && conversationId.isEmpty())
        return QStringLiteral("Error: conversation-scoped recall requires an "
                              "active conversation id.");

    const auto matches = MemoryToolUtils::search(
        m_memoriesDir, scope, conversationId, query, filePattern, limit);
    if (matches.isEmpty())
        return QStringLiteral("No memory matched query: %1 (scope=%2)")
            .arg(query, scope);

    QStringList hits;
    for (const auto &match : matches)
    {
        const QFileInfo info(match.path);
        hits.append(QStringLiteral("## %1\nscope: %2\nscore: %3\npath: %4\n%5")
                        .arg(info.fileName(), match.scope)
                        .arg(QString::number(match.score, 'f', 1),
                             match.relativePath, match.excerpt));
    }
    return QStringLiteral(
               "Memory search results for query: %1\nscope: %2\n\n%3")
        .arg(query, scope, hits.join(QStringLiteral("\n\n")));
}
