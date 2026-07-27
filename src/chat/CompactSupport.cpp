#include "CompactSupport.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace CompactSupport
{
namespace
{

int charsPerToken() { return 4; }

} // namespace

QString summarizationPrompt()
{
    return QStringLiteral(
        "You are performing a CONTEXT CHECKPOINT COMPACTION. Create a handoff "
        "summary for another LLM that will resume the task.\n\n"
        "Include:\n"
        "- Current progress and key decisions made\n"
        "- Important context, constraints, or user preferences\n"
        "- What remains to be done (clear next steps)\n"
        "- Any critical data, examples, or references needed to continue\n\n"
        "Be concise, structured, and focused on helping the next LLM "
        "seamlessly "
        "continue the work.");
}

QString summaryPrefix()
{
    return QStringLiteral(
        "Another language model started to solve this problem and produced a "
        "summary of its thinking process. "
        "You also have access to the state of the tools that were used by that "
        "language model. "
        "Use this to build on the work that has already been done and avoid "
        "duplicating work. "
        "Here is the summary produced by the other language model, use the "
        "information in this summary to assist with your own analysis:");
}

int modelContextWindow(const QString &model)
{
    const QString m = model.trimmed().toLower();
    if (m.contains(QStringLiteral("gpt-5")) ||
        m.contains(QStringLiteral("gpt-4.1")) ||
        m.contains(QStringLiteral("gpt-4o")) ||
        m.contains(QStringLiteral("o3")) || m.contains(QStringLiteral("o4")))
    {
        return 128000;
    }
    if (m.contains(QStringLiteral("deepseek")) ||
        m.contains(QStringLiteral("qwen")))
    {
        return 64000;
    }
    if (m.contains(QStringLiteral("claude")))
    {
        return 200000;
    }
    return kDefaultContextWindow;
}

int usableContextWindow(const QString &model)
{
    return int(double(modelContextWindow(model)) * 0.85);
}

int roughTokenCount(const QString &text)
{
    if (text.isEmpty())
        return 0;
    return qMax(1, text.size() / charsPerToken());
}

int roughTokenCount(const QJsonArray &messages)
{
    int total = 0;
    for (const QJsonValue &value : messages)
    {
        const QJsonObject obj = value.toObject();
        total += 12; // per-message framing
        total += roughTokenCount(obj.value(QStringLiteral("role")).toString());

        const QJsonValue contentValue = obj.value(QStringLiteral("content"));
        if (contentValue.isString())
        {
            total += roughTokenCount(contentValue.toString());
        }
        else if (contentValue.isArray())
        {
            const QJsonArray parts = contentValue.toArray();
            for (const QJsonValue &partValue : parts)
            {
                const QJsonObject part = partValue.toObject();
                const QString type =
                    part.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("text"))
                {
                    total += roughTokenCount(
                        part.value(QStringLiteral("text")).toString());
                }
                else if (type == QStringLiteral("image_url"))
                {
                    total += 768;
                }
                else
                {
                    total += roughTokenCount(QString::fromUtf8(
                        QJsonDocument(part).toJson(QJsonDocument::Compact)));
                }
            }
        }

        if (obj.contains(QStringLiteral("tool_calls")))
            total += roughTokenCount(QString::fromUtf8(
                QJsonDocument(obj.value(QStringLiteral("tool_calls")).toArray())
                    .toJson(QJsonDocument::Compact)));
    }
    return total;
}

QString summarizeMessageForTranscript(const QString &role, const QString &text,
                                      const QStringList &imagePaths)
{
    QString out = QStringLiteral("[%1]\n").arg(role);
    if (!imagePaths.isEmpty())
        out += QStringLiteral("(attached images: %1)\n").arg(imagePaths.size());
    out += text.trimmed();
    return out.trimmed();
}

QString trimToTokenBudget(const QString &text, int maxTokens)
{
    if (roughTokenCount(text) <= maxTokens)
        return text;
    const int maxChars = qMax(0, maxTokens * charsPerToken());
    return text.left(maxChars);
}

} // namespace CompactSupport
