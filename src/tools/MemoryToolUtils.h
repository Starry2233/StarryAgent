#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace MemoryToolUtils
{

struct Match
{
    QString path;
    QString relativePath;
    QString scope;
    double score = 0.0;
    QString excerpt;
};

QString normalizeScope(const QString &scope);
QString resolveScopeDir(const QString &memoriesDir, const QString &scope,
                        const QString &conversationId);
QString sanitizeKey(const QString &key);
QString buildMemoryDocument(const QString &title, const QString &content,
                            const QStringList &tags, const QString &scope,
                            const QString &conversationId);
QVector<Match> search(const QString &memoriesDir, const QString &scope,
                      const QString &conversationId, const QString &query,
                      const QString &filePattern, int limit);

} // namespace MemoryToolUtils
