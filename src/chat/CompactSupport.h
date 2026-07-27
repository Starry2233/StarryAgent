#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

namespace CompactSupport
{

inline constexpr int kDefaultContextWindow = 32768;
inline constexpr int kCompactionTimeoutMs = 45000;

QString summarizationPrompt();
QString summaryPrefix();
int modelContextWindow(const QString &model);
int usableContextWindow(const QString &model);
int roughTokenCount(const QString &text);
int roughTokenCount(const QJsonArray &messages);
QString summarizeMessageForTranscript(const QString &role, const QString &text,
                                      const QStringList &imagePaths = {});
QString trimToTokenBudget(const QString &text, int maxTokens);

} // namespace CompactSupport
