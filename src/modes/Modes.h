#pragma once

#include <QString>

// Three agent modes. Only the initial system prompt differs — tools, settings,
// and the streaming pipeline are identical across modes. The picker is the
// hero moment of the UI (CLAUDE.md: "the one thing users remember").
namespace Modes
{

enum class Mode
{
    Agent,
    Coding,
    Pal
};

inline const char *id(Mode m)
{
    switch (m)
    {
    case Mode::Agent:
        return "agent";
    case Mode::Coding:
        return "coding";
    case Mode::Pal:
        return "pal";
    }
    return "agent";
}

inline Mode fromId(const QString &s)
{
    if (s == "coding")
        return Mode::Coding;
    if (s == "pal")
        return Mode::Pal;
    return Mode::Agent;
}

inline QString label(Mode m)
{
    switch (m)
    {
    case Mode::Agent:
        return QStringLiteral("Agent");
    case Mode::Coding:
        return QStringLiteral("Coding");
    case Mode::Pal:
        return QStringLiteral("Pal");
    }
    return QStringLiteral("Agent");
}

inline QString description(Mode m)
{
    switch (m)
    {
    case Mode::Agent:
        return QStringLiteral("A general-purpose agent. Free to call tools, "
                              "browse, and act on your behalf.");
    case Mode::Coding:
        return QStringLiteral(
            "A code assistant. Reads, edits, and writes files; "
            "runs commands. Prefers targeted edits.");
    case Mode::Pal:
        return QStringLiteral("A warm conversational companion. Chat-focused; "
                              "tools available but used sparingly.");
    }
    return {};
}

// Base system prompt for the mode. `indexMd` is the contents of the user's
// <root>/index.md, appended so the user can extend the prompt per CLAUDE.md.
inline QString systemPrompt(Mode m, const QString &indexMd,
                            const QString &skillsMd = {})
{
    const QString markdownExtensionRule = QStringLiteral(
        "Markdown extension available in this UI:\n"
        "- For short inline-highlighted code chips, use exactly `{`code` "
        "ext=language}`.\n"
        "- Only `ext=` is supported. Example: {`print(\"hi\")` ext=python}\n"
        "- Use this syntax only outside fenced code blocks.\n"
        "- If you need a literal `{` or `}` outside code blocks, output "
        "`&#123;` "
        "and `&#125;` instead.\n");

    QString base;
    switch (m)
    {
    case Mode::Agent:
        base =
            QStringLiteral(
                "You are StarryAgent in Agent mode — a general-purpose AI "
                "agent.\n"
                "You operate in a workspace directory and have tools to read, "
                "edit, and "
                "write files, run shell commands, and fetch web content. Use "
                "them "
                "whenever they help accomplish the user's goal.\n"
                "Be direct and concrete. When a task is ambiguous, ask one "
                "sharp "
                "question rather than guessing. When you act, say briefly what "
                "you "
                "did and why. Prefer fewer, higher-signal messages over "
                "verbose "
                "narration.\n"
                "Tool calls are gated by user approval unless bypass is on. "
                "Never "
                "claim an action without actually calling the relevant tool.\n"
                "The memory store is not preloaded. If you need past saved "
                "memory, "
                "call the `recall_memory` tool explicitly. If a user asks you "
                "to "
                "remember something for later, call `write_memory` "
                "explicitly.\n") +
            markdownExtensionRule;
        break;
    case Mode::Coding:
        base =
            QStringLiteral(
                "You are StarryAgent in Coding mode — a code assistant in the "
                "Claude-Code tradition.\n"
                "You can read, edit, and overwrite files, and run shell "
                "commands "
                "in "
                "the workspace. Prefer targeted edits (the `edit` tool) over "
                "full "
                "rewrites; use `overwrite` only for new files or complete "
                "rewrites.\n"
                "Read before you edit. Make the smallest correct change. "
                "Explain "
                "the "
                "fix concisely; do not refactor surrounding code unless asked. "
                "Run "
                "build/test commands to verify when reasonable.\n"
                "Tool calls are gated by user approval unless bypass is on.\n"
                "The memory store is not preloaded. If you need past saved "
                "memory, "
                "call the `recall_memory` tool explicitly. If a user asks you "
                "to "
                "remember something for later, call `write_memory` "
                "explicitly.\n") +
            markdownExtensionRule;
        break;
    case Mode::Pal:
        base = QStringLiteral(
                   "You are StarryAgent in Pal mode — a warm, thoughtful "
                   "conversational "
                   "companion.\n"
                   "Lead with the conversation. Tools are available but should "
                   "stay "
                   "out "
                   "of the way unless the user clearly wants you to act on "
                   "something. "
                   "Write prose that reads like typeset text: clear, warm, no "
                   "padding.\n"
                   "The memory store is not preloaded. If you need past saved "
                   "memory, "
                   "call the `recall_memory` tool explicitly. If a user asks "
                   "you to "
                   "remember something for later, call `write_memory` "
                   "explicitly.\n") +
               markdownExtensionRule;
        break;
    }

    if (!indexMd.trimmed().isEmpty())
        base += QStringLiteral("\n\n--- User notes (index.md) ---\n") + indexMd;
    if (!skillsMd.trimmed().isEmpty())
        base += QStringLiteral("\n\n--- Skills ---\n") + skillsMd;
    return base;
}

} // namespace Modes
