#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

// Segment types for parsed markdown text.
enum class SegmentKind
{
    Markdown,
    CodeBlock,
    Latex,
    Table
};

// MarkdownParser — lightweight markdown tokenizer. Splits raw text into
// segments:
//   - ```lang ... ``` → CodeBlock
//   - $$...$$ or $...$ → Latex
//   - markdown tables → Table (rendered as plain monospace text)
//   - everything else → Markdown (rendered by QtQuick Text with MarkdownText)
//
// This is intentionally simple: no full AST. We just need to identify fence
// boundaries so code blocks and math get their own visual treatment.
class MarkdownParser : public QObject
{
    Q_OBJECT
  public:
    explicit MarkdownParser(QObject *parent = nullptr);

    // Parse `raw` into a JSON array of segments. Each segment is:
    //   { "kind": "markdown",  "text": "..." }
    //   { "kind": "code",      "lang": "cpp", "text": "..." }
    //   { "kind": "table",     "text": "| a | b |\n|---|---|" }
    //   { "kind": "latex",     "block": true, "text": "..." }   // block =
    //   $$...$$ { "kind": "latex",     "block": false, "text": "..." }  //
    //   inline = $...$
    Q_INVOKABLE QJsonArray parse(const QString &raw);
};
