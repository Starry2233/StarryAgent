#include "MemoryToolUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>

namespace MemoryToolUtils
{
namespace
{

QString excerptAround(const QString &haystack, int pos, int len)
{
    const int radius = 120;
    const int start = qMax(0, pos - radius);
    const int end = qMin(haystack.size(), pos + len + radius);
    QString out = haystack.mid(start, end - start).trimmed();
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    out.replace(QRegularExpression(QStringLiteral("\\s+")),
                QStringLiteral(" "));
    return out;
}

bool isSubsequence(const QString &needle, const QString &haystack)
{
    if (needle.isEmpty())
        return true;
    int j = 0;
    for (const QChar ch : haystack)
    {
        if (ch == needle[j])
        {
            ++j;
            if (j >= needle.size())
                return true;
        }
    }
    return false;
}

int levenshteinDistance(const QString &a, const QString &b)
{
    if (a.isEmpty())
        return b.size();
    if (b.isEmpty())
        return a.size();

    QVector<int> prev(b.size() + 1);
    QVector<int> curr(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j)
        prev[j] = j;

    for (int i = 1; i <= a.size(); ++i)
    {
        curr[0] = i;
        for (int j = 1; j <= b.size(); ++j)
        {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] =
                std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        prev.swap(curr);
    }
    return prev[b.size()];
}

double lineScore(const QString &query, const QString &line)
{
    if (query.isEmpty() || line.isEmpty())
        return 0.0;

    const QString q = query.toLower();
    const QString l = line.toLower();
    const int exactPos = l.indexOf(q);
    if (exactPos >= 0)
        return 1000.0 - exactPos;

    double score = 0.0;
    const QStringList qTokens =
        q.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    int tokenHits = 0;
    for (const QString &token : qTokens)
    {
        if (l.contains(token))
            ++tokenHits;
    }
    if (!qTokens.isEmpty())
        score += 200.0 * double(tokenHits) / double(qTokens.size());

    if (isSubsequence(q, l))
        score += 80.0;

    const int compareLen = qBound(0, q.size() + 8, l.size());
    if (compareLen > 0)
    {
        const int dist = levenshteinDistance(q, l.left(compareLen));
        score += qMax(0.0, 60.0 - double(dist) * 6.0);
    }

    return score;
}

QStringList supportedPatterns()
{
    return {QStringLiteral("*.md"),   QStringLiteral("*.txt"),
            QStringLiteral("*.json"), QStringLiteral("*.jsonc"),
            QStringLiteral("*.yml"),  QStringLiteral("*.yaml")};
}

} // namespace

QString normalizeScope(const QString &scope)
{
    const QString normalized = scope.trimmed().toLower();
    return normalized == QStringLiteral("global")
               ? normalized
               : QStringLiteral("conversation");
}

QString resolveScopeDir(const QString &memoriesDir, const QString &scope,
                        const QString &conversationId)
{
    const QString normalizedScope = normalizeScope(scope);
    if (normalizedScope == QStringLiteral("global"))
        return QDir(memoriesDir).filePath(QStringLiteral("global"));
    if (conversationId.trimmed().isEmpty())
        return QString();
    return QDir(memoriesDir)
        .filePath(QStringLiteral("conversations/%1").arg(conversationId));
}

QString sanitizeKey(const QString &key)
{
    QString out = key.trimmed();
    out.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                QStringLiteral("-"));
    out.replace(QRegularExpression(QStringLiteral("\\s+")),
                QStringLiteral("-"));
    out.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return out.left(96);
}

QString buildMemoryDocument(const QString &title, const QString &content,
                            const QStringList &tags, const QString &scope,
                            const QString &conversationId)
{
    QStringList lines;
    if (!title.trimmed().isEmpty())
        lines << QStringLiteral("# %1").arg(title.trimmed());
    lines << QStringLiteral("scope: %1").arg(normalizeScope(scope));
    if (!conversationId.trimmed().isEmpty())
        lines << QStringLiteral("conversation_id: %1")
                     .arg(conversationId.trimmed());
    if (!tags.isEmpty())
        lines
            << QStringLiteral("tags: %1").arg(tags.join(QStringLiteral(", ")));
    lines << QStringLiteral("created_at: %1")
                 .arg(QString::fromLatin1(QDateTime::currentDateTimeUtc()
                                              .toString(Qt::ISODate)
                                              .toUtf8()));
    lines << QString();
    lines << content;
    return lines.join(QLatin1Char('\n'));
}

QVector<Match> search(const QString &memoriesDir, const QString &scope,
                      const QString &conversationId, const QString &query,
                      const QString &filePattern, int limit)
{
    QVector<Match> matches;
    const QString scopeDir =
        resolveScopeDir(memoriesDir, scope, conversationId);
    if (scopeDir.isEmpty())
        return matches;

    QDirIterator it(scopeDir, supportedPatterns(), QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = it.next();
        const QFileInfo info(path);
        if (!filePattern.trimmed().isEmpty() &&
            !info.fileName().contains(filePattern, Qt::CaseInsensitive) &&
            !info.filePath().contains(filePattern, Qt::CaseInsensitive))
        {
            continue;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString content = QTextStream(&file).readAll();
        const QStringList lines = content.split(QLatin1Char('\n'));

        double bestScore = lineScore(query, info.fileName());
        QString bestExcerpt = content.left(240).trimmed();
        int bestPos = content.indexOf(query, 0, Qt::CaseInsensitive);
        if (bestPos >= 0)
            bestExcerpt = excerptAround(content, bestPos, query.size());

        for (const QString &line : lines)
        {
            const double score = lineScore(query, line);
            if (score > bestScore)
            {
                bestScore = score;
                const int pos = content.indexOf(line);
                bestExcerpt = pos >= 0
                                  ? excerptAround(content, pos, line.size())
                                  : line.trimmed();
            }
        }

        if (bestScore <= 0.0)
            continue;

        if (bestExcerpt.isEmpty())
            bestExcerpt = content.left(240).trimmed();

        Match match;
        match.path = path;
        match.relativePath = QDir(scopeDir).relativeFilePath(path);
        match.scope = normalizeScope(scope);
        match.score = bestScore;
        match.excerpt = bestExcerpt;
        matches.append(match);
    }

    std::sort(matches.begin(), matches.end(),
              [](const Match &a, const Match &b)
              {
                  if (a.score == b.score)
                      return a.relativePath < b.relativePath;
                  return a.score > b.score;
              });

    if (matches.size() > limit)
        matches.resize(limit);
    return matches;
}

} // namespace MemoryToolUtils
