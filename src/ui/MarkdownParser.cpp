#include "MarkdownParser.h"

#include <QJsonDocument>

#include "core/DebugTrace.h"

namespace
{
int findUnescaped(const QString &text, QChar needle, int from)
{
    for (int i = from; i < text.length(); ++i)
    {
        if (text[i] != needle)
            continue;
        if (i > 0 && text[i - 1] == QLatin1Char('\\'))
            continue;
        return i;
    }
    return -1;
}

QString extractImageUrl(QString target)
{
    target = target.trimmed();
    if (target.startsWith(QLatin1Char('<')))
    {
        const int close = target.indexOf(QLatin1Char('>'));
        if (close > 1)
            return target.mid(1, close - 1).trimmed();
    }

    const int firstSpace = target.indexOf(QLatin1Char(' '));
    if (firstSpace > 0)
        target = target.left(firstSpace);
    return target.trimmed();
}

int lineEndAt(const QString &text, int from)
{
    const int end = text.indexOf(QLatin1Char('\n'), from);
    return end < 0 ? text.length() : end;
}

QString lineAt(const QString &text, int from)
{
    const int end = lineEndAt(text, from);
    return text.mid(from, end - from);
}

int nextLineStart(const QString &text, int from)
{
    const int end = lineEndAt(text, from);
    return end < text.length() ? end + 1 : text.length();
}

bool isMarkdownTableRow(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return false;
    int pipes = 0;
    for (const QChar ch : trimmed)
    {
        if (ch == QLatin1Char('|'))
            ++pipes;
    }
    return pipes >= 2;
}

bool isMarkdownTableSeparator(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || !trimmed.contains(QLatin1Char('|')))
        return false;

    int dashCount = 0;
    for (const QChar ch : trimmed)
    {
        if (ch == QLatin1Char('-'))
        {
            ++dashCount;
            continue;
        }
        if (ch == QLatin1Char('|') || ch == QLatin1Char(':') ||
            ch == QLatin1Char(' ') || ch == QLatin1Char('\t'))
        {
            continue;
        }
        return false;
    }
    return dashCount >= 3;
}

bool isMarkdownHorizontalRule(const QString &line)
{
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return false;

    trimmed.remove(QLatin1Char(' '));
    trimmed.remove(QLatin1Char('\t'));
    if (trimmed.size() < 3)
        return false;

    const QChar marker = trimmed.front();
    if (marker != QLatin1Char('-') && marker != QLatin1Char('*') &&
        marker != QLatin1Char('_'))
    {
        return false;
    }

    for (const QChar ch : trimmed)
    {
        if (ch != marker)
            return false;
    }
    return true;
}

} // namespace

MarkdownParser::MarkdownParser(QObject *parent) : QObject(parent) {}

QJsonArray MarkdownParser::parse(const QString &raw)
{
    QJsonArray segments;
    if (raw.isEmpty())
        return segments;

    DebugTrace::verbose("markdown-parser",
                        QStringLiteral("parse rawLen=%1 lines=%2")
                            .arg(raw.size())
                            .arg(raw.count(QLatin1Char('\n')) + 1));

    const int n = raw.length();
    int i = 0;

    // Accumulator for markdown text that hasn't been flushed yet.
    QString mdAccum;
    auto flushMd = [&]()
    {
        if (!mdAccum.isEmpty())
        {
            QJsonObject seg;
            seg.insert("kind", "markdown");
            seg.insert("text", mdAccum);
            segments.append(seg);
            DebugTrace::verbose("markdown-parser",
                                QStringLiteral("segment kind=markdown len=%1")
                                    .arg(mdAccum.size()));
            mdAccum.clear();
        }
    };

    while (i < n)
    {
        const bool startOfLine = (i == 0 || raw[i - 1] == QLatin1Char('\n'));

        // --- Code fence: ```lang at start of line ---
        if (raw[i] == QLatin1Char('`') && i + 2 < n &&
            raw[i + 1] == QLatin1Char('`') && raw[i + 2] == QLatin1Char('`'))
        {
            // Make sure we're at the start of a line.
            bool atFenceLineStart = (i == 0);
            if (!atFenceLineStart)
            {
                // Check if everything before this point on the current line is
                // whitespace.
                int lineStart = raw.lastIndexOf(QLatin1Char('\n'), i - 1);
                lineStart = (lineStart < 0) ? 0 : lineStart + 1;
                bool allSpace = true;
                for (int k = lineStart; k < i; ++k)
                {
                    if (raw[k] != QLatin1Char(' ') &&
                        raw[k] != QLatin1Char('\t'))
                    {
                        allSpace = false;
                        break;
                    }
                }
                atFenceLineStart = allSpace;
            }
            if (atFenceLineStart)
            {
                // Consume the opening ```.
                i += 3;
                // Capture optional language hint.
                QString lang;
                while (i < n && raw[i] != QLatin1Char('\n') &&
                       raw[i] != QLatin1Char('`'))
                {
                    lang.append(raw[i]);
                    ++i;
                }
                // Consume the rest of the opening line.
                if (i < n && raw[i] == QLatin1Char('\n'))
                    ++i;

                // Find closing ```.
                int close = raw.indexOf(QLatin1String("```"), i);
                QString code;
                if (close >= 0)
                {
                    code = raw.mid(i, close - i);
                    i = close + 3;
                    // Consume trailing newline if present.
                    if (i < n && raw[i] == QLatin1Char('\n'))
                        ++i;
                }
                else
                {
                    // Unterminated fence — consume to end.
                    code = raw.mid(i);
                    i = n;
                }

                flushMd();
                QJsonObject seg;
                seg.insert("kind", "code");
                seg.insert("lang", lang.trimmed());
                seg.insert("text", code);
                segments.append(seg);
                DebugTrace::verbose(
                    "markdown-parser",
                    QStringLiteral("segment kind=code lang=%1 len=%2")
                        .arg(lang.trimmed())
                        .arg(code.size()));
                continue;
            }
        }

        // --- LaTeX block: $$ at start of line ---
        if (raw[i] == QLatin1Char('$') && i + 1 < n &&
            raw[i + 1] == QLatin1Char('$'))
        {
            bool atMathLineStart = (i == 0);
            if (!atMathLineStart)
            {
                int lineStart = raw.lastIndexOf(QLatin1Char('\n'), i - 1);
                lineStart = (lineStart < 0) ? 0 : lineStart + 1;
                bool allSpace = true;
                for (int k = lineStart; k < i; ++k)
                {
                    if (raw[k] != QLatin1Char(' ') &&
                        raw[k] != QLatin1Char('\t'))
                    {
                        allSpace = false;
                        break;
                    }
                }
                atMathLineStart = allSpace;
            }
            if (atMathLineStart)
            {
                i += 2; // skip $$
                int close = raw.indexOf(QLatin1String("$$"), i);
                QString math;
                if (close >= 0)
                {
                    math = raw.mid(i, close - i);
                    i = close + 2;
                    if (i < n && raw[i] == QLatin1Char('\n'))
                        ++i;
                }
                else
                {
                    math = raw.mid(i);
                    i = n;
                }
                flushMd();
                QJsonObject seg;
                seg.insert("kind", "latex");
                seg.insert("block", true);
                seg.insert("text", math.trimmed());
                segments.append(seg);
                DebugTrace::verbose(
                    "markdown-parser",
                    QStringLiteral("segment kind=latex block=1 len=%1")
                        .arg(math.trimmed().size()));
                continue;
            }
        }

        // --- Markdown table: header row + separator row at start of line ---
        //
        // Qt's native MarkdownText table path has proven expensive for some
        // restored histories. Keep the table intact, but render it as a plain
        // monospace block in QML so it cannot allocate a complex rich-text
        // table scene graph.
        if (startOfLine)
        {
            const QString firstLine = lineAt(raw, i);
            const int secondStart = nextLineStart(raw, i);
            if (secondStart < n)
            {
                const QString secondLine = lineAt(raw, secondStart);
                if (isMarkdownTableRow(firstLine) &&
                    isMarkdownTableSeparator(secondLine))
                {
                    int end = nextLineStart(raw, secondStart);
                    while (end < n)
                    {
                        const QString rowLine = lineAt(raw, end);
                        if (!isMarkdownTableRow(rowLine))
                            break;
                        end = nextLineStart(raw, end);
                    }

                    flushMd();
                    QJsonObject seg;
                    seg.insert("kind", "table");
                    seg.insert("text", raw.mid(i, end - i).trimmed());
                    segments.append(seg);
                    DebugTrace::verbose(
                        "markdown-parser",
                        QStringLiteral("segment kind=table len=%1")
                            .arg(raw.mid(i, end - i).trimmed().size()));
                    i = end;
                    continue;
                }
            }
        }

        // --- Horizontal rule: --- / *** / ___ on its own line ---
        if (startOfLine)
        {
            const QString ruleLine = lineAt(raw, i);
            if (isMarkdownHorizontalRule(ruleLine))
            {
                flushMd();
                QJsonObject seg;
                seg.insert("kind", "rule");
                segments.append(seg);
                DebugTrace::verbose("markdown-parser",
                                    QStringLiteral("segment kind=rule"));
                i = nextLineStart(raw, i);
                continue;
            }
        }

        // --- Inline LaTeX: $...$ (not preceded by backslash) ---
        if (raw[i] == QLatin1Char('$') &&
            (i == 0 || raw[i - 1] != QLatin1Char('\\')))
        {
            int open = i;
            ++i; // skip opening $
            // Find closing $ (not escaped).
            int close = -1;
            while (i < n)
            {
                if (raw[i] == QLatin1Char('$') &&
                    (i == 0 || raw[i - 1] != QLatin1Char('\\')))
                {
                    close = i;
                    break;
                }
                // Don't cross line boundaries for inline math.
                if (raw[i] == QLatin1Char('\n'))
                    break;
                ++i;
            }
            if (close > open + 1)
            {
                QString math = raw.mid(open + 1, close - open - 1);
                // Check it's not a double-dollar (already handled above).
                if (!math.startsWith(QLatin1Char('$')))
                {
                    flushMd();
                    QJsonObject seg;
                    seg.insert("kind", "latex");
                    seg.insert("block", false);
                    seg.insert("text", math);
                    segments.append(seg);
                    DebugTrace::verbose(
                        "markdown-parser",
                        QStringLiteral("segment kind=latex block=0 len=%1")
                            .arg(math.size()));
                    i = close + 1;
                    continue;
                }
            }
            // If we fell through, treat the $ as literal text.
            mdAccum.append(QLatin1Char('$'));
            continue;
        }

        // --- Markdown image: ![alt](url) ---
        if (raw[i] == QLatin1Char('!') && i + 1 < n &&
            raw[i + 1] == QLatin1Char('[') &&
            (i == 0 || raw[i - 1] != QLatin1Char('\\')))
        {
            const int altEnd = findUnescaped(raw, QLatin1Char(']'), i + 2);
            if (altEnd > i + 1 && altEnd + 1 < n &&
                raw[altEnd + 1] == QLatin1Char('('))
            {
                const int urlEnd =
                    findUnescaped(raw, QLatin1Char(')'), altEnd + 2);
                if (urlEnd > altEnd + 2)
                {
                    const QString alt = raw.mid(i + 2, altEnd - (i + 2));
                    const QString url = extractImageUrl(
                        raw.mid(altEnd + 2, urlEnd - (altEnd + 2)));
                    if (!url.isEmpty())
                    {
                        flushMd();
                        QJsonObject seg;
                        seg.insert("kind", "image");
                        seg.insert("alt", alt);
                        seg.insert("url", url);
                        segments.append(seg);
                        DebugTrace::verbose(
                            "markdown-parser",
                            QStringLiteral(
                                "segment kind=image altLen=%1 url=%2")
                                .arg(alt.size())
                                .arg(url));
                        i = urlEnd + 1;
                        continue;
                    }
                }
            }
        }

        mdAccum.append(raw[i]);
        ++i;
    }

    flushMd();
    DebugTrace::verbose(
        "markdown-parser",
        QStringLiteral("parse done segments=%1").arg(segments.size()));
    return segments;
}
