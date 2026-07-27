#include "CodeHighlighter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>

#include "core/DebugTrace.h"
#include "definition.h"
#include "repository.h"
#include "syntaxhighlighter.h"
#include "theme.h"

namespace
{
QString syntaxDataDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("syntax-highlighting-data")),
        QDir(appDir).filePath(QStringLiteral("../syntax-highlighting-data")),
        QDir::cleanPath(QDir(appDir).filePath(
            QStringLiteral("../../../../external/syntax-highlighting/data"))),
        QDir::cleanPath(QDir::currentPath() +
                        QStringLiteral("/external/syntax-highlighting/data")),
        QStringLiteral("E:/StarryAgent/external/syntax-highlighting/data"),
    };

    for (const QString &candidate : candidates)
    {
        if (QFileInfo::exists(
                QDir(candidate).filePath(QStringLiteral("syntax"))) &&
            QFileInfo::exists(
                QDir(candidate).filePath(QStringLiteral("themes"))))
        {
            return QDir::cleanPath(candidate);
        }
    }
    return QString();
}

KSyntaxHighlighting::Definition
definitionForLanguage(KSyntaxHighlighting::Repository &repo,
                      const QString &language)
{
    const QString normalized = language.trimmed();
    if (normalized.isEmpty())
        return {};

    auto def = repo.definitionForName(normalized);
    if (def.isValid())
        return def;

    const QString lower = normalized.toLower();
    if (lower == QStringLiteral("cpp") || lower == QStringLiteral("cxx") ||
        lower == QStringLiteral("cc"))
        return repo.definitionForName(QStringLiteral("C++"));
    if (lower == QStringLiteral("js"))
        return repo.definitionForName(QStringLiteral("JavaScript"));
    if (lower == QStringLiteral("ts"))
        return repo.definitionForName(QStringLiteral("TypeScript"));
    if (lower == QStringLiteral("sh") || lower == QStringLiteral("shell") ||
        lower == QStringLiteral("zsh"))
        return repo.definitionForName(QStringLiteral("Bash"));
    if (lower == QStringLiteral("yml"))
        return repo.definitionForName(QStringLiteral("YAML"));
    if (lower == QStringLiteral("md"))
        return repo.definitionForName(QStringLiteral("Markdown"));
    if (lower == QStringLiteral("plaintext") || lower == QStringLiteral("text"))
        return {};
    return repo.definitionForFileName(QStringLiteral("sample.") + lower);
}

void resetDocumentFormatting(QTextDocument *document, bool dark)
{
    if (!document)
        return;

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);

    QTextCharFormat format;
    format.setForeground(QColor(dark ? QStringLiteral("#D4D4D4")
                                     : QStringLiteral("#1C1916")));
    format.setBackground(Qt::transparent);
    cursor.setCharFormat(format);
    cursor.clearSelection();
    cursor.endEditBlock();
}

QString renderInlineExtensionSyntax(const QString &markdown,
                                    const CodeHighlighter *self, bool dark)
{
    QString out;
    out.reserve(markdown.size());

    for (int i = 0; i < markdown.size();)
    {
        if (markdown[i] == QLatin1Char('{') && i + 2 < markdown.size() &&
            markdown[i + 1] == QLatin1Char('`'))
        {
            int j = i + 2;
            QString code;
            while (j < markdown.size())
            {
                const QChar ch = markdown[j];
                if (ch == QLatin1Char('`') &&
                    markdown[j - 1] != QLatin1Char('\\'))
                    break;
                if (ch == QLatin1Char('\n'))
                    break;
                code.append(ch);
                ++j;
            }

            if (j < markdown.size() && markdown[j] == QLatin1Char('`'))
            {
                ++j;
                while (j < markdown.size() && markdown[j].isSpace() &&
                       markdown[j] != QLatin1Char('\n'))
                    ++j;

                QString lang;
                if (markdown.mid(j, 4) == QLatin1String("ext="))
                {
                    j += 4;
                }
                else if (markdown.mid(j, 10) == QLatin1String("extension="))
                {
                    j += 10;
                }
                else if (markdown.mid(j, 10) == QLatin1String("extention="))
                {
                    j += 10;
                }
                else
                {
                    out.append(markdown[i]);
                    ++i;
                    continue;
                }

                while (j < markdown.size())
                {
                    const QChar ch = markdown[j];
                    if (ch == QLatin1Char('}'))
                        break;
                    if (ch == QLatin1Char('\n'))
                    {
                        lang.clear();
                        break;
                    }
                    lang.append(ch);
                    ++j;
                }

                if (j < markdown.size() && markdown[j] == QLatin1Char('}') &&
                    !lang.trimmed().isEmpty())
                {
                    out.append(
                        self->inlineCodeHtml(code, lang.trimmed(), dark));
                    i = j + 1;
                    continue;
                }
            }
        }

        out.append(markdown[i]);
        ++i;
    }

    return out;
}

class SharedRepository
{
  public:
    SharedRepository()
    {
        const QString dataDir = syntaxDataDir();
        if (!dataDir.isEmpty())
            repo.addCustomSearchPath(dataDir);
    }

    KSyntaxHighlighting::Repository repo;
};

SharedRepository &sharedRepository()
{
    static SharedRepository instance;
    return instance;
}
} // namespace

CodeHighlighter::CodeHighlighter(QObject *parent) : QObject(parent) {}

void CodeHighlighter::applyToDocument(QObject *quickTextDocument,
                                      const QString &language, bool dark)
{
    auto *quickDoc = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    if (!quickDoc || !quickDoc->textDocument())
        return;

    QTextDocument *document = quickDoc->textDocument();
    auto &repo = sharedRepository().repo;
    const auto def = definitionForLanguage(repo, language);

    if (!def.isValid())
    {
        if (auto highlighter = m_highlighters.take(document); highlighter)
            highlighter->deleteLater();
        resetDocumentFormatting(document, dark);
        DebugTrace::verbose(
            "code-highlight",
            QStringLiteral("lang=%1 def=(invalid) doc=%2")
                .arg(language)
                .arg(reinterpret_cast<quintptr>(document), 0, 16));
        return;
    }

    KSyntaxHighlighting::SyntaxHighlighter *highlighter =
        m_highlighters.value(document);
    if (!highlighter)
    {
        highlighter = new KSyntaxHighlighting::SyntaxHighlighter(document);
        highlighter->setParent(document);
        m_highlighters.insert(document, highlighter);
        connect(document, &QObject::destroyed, this,
                [this, document] { m_highlighters.remove(document); });
    }

    KSyntaxHighlighting::Theme theme = repo.theme(
        dark ? QStringLiteral("GitHub Dark") : QStringLiteral("GitHub Light"));
    if (!theme.isValid())
    {
        theme = repo.defaultTheme(
            dark ? KSyntaxHighlighting::Repository::DarkTheme
                 : KSyntaxHighlighting::Repository::LightTheme);
    }

    highlighter->setTheme(theme);
    highlighter->setDefinition(def);
    highlighter->rehighlight();

    DebugTrace::verbose("code-highlight",
                        QStringLiteral("lang=%1 def=%2 doc=%3 chars=%4")
                            .arg(language, def.name())
                            .arg(reinterpret_cast<quintptr>(document), 0, 16)
                            .arg(document->characterCount()));
}

QString CodeHighlighter::inlineCodeHtml(const QString &code,
                                        const QString &language,
                                        bool dark) const
{
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setPlainText(code);

    auto &repo = sharedRepository().repo;
    const auto def = definitionForLanguage(repo, language);
    const QString definitionName = def.name();
    const bool valid = def.isValid();
    if (def.isValid())
    {
        KSyntaxHighlighting::SyntaxHighlighter highlighter(&document);
        KSyntaxHighlighting::Theme theme =
            repo.theme(dark ? QStringLiteral("GitHub Dark")
                            : QStringLiteral("GitHub Light"));
        if (!theme.isValid())
        {
            theme = repo.defaultTheme(
                dark ? KSyntaxHighlighting::Repository::DarkTheme
                     : KSyntaxHighlighting::Repository::LightTheme);
        }
        highlighter.setTheme(theme);
        highlighter.setDefinition(def);
        highlighter.rehighlight();
    }

    QString html = document.toHtml();
    const int bodyStart = html.indexOf(QStringLiteral("<body"));
    if (bodyStart >= 0)
    {
        const int contentStart = html.indexOf(QLatin1Char('>'), bodyStart);
        const int bodyEnd =
            html.indexOf(QStringLiteral("</body>"), contentStart);
        if (contentStart >= 0 && bodyEnd > contentStart)
            html = html.mid(contentStart + 1, bodyEnd - contentStart - 1)
                       .trimmed();
    }
    html.remove(QRegularExpression(QStringLiteral("^<p[^>]*>")));
    html.remove(QRegularExpression(QStringLiteral("</p>$")));
    html.replace(QStringLiteral("\n"), QStringLiteral(""));

    const QString bg =
        dark ? QStringLiteral("#201C18") : QStringLiteral("#F3EBDD");
    const QString border =
        dark ? QStringLiteral("#3B342C") : QStringLiteral("#D9CFBC");
    const QString textColor =
        dark ? QStringLiteral("#D4D4D4") : QStringLiteral("#1C1916");

    DebugTrace::verbose("code-highlight-inline",
                        QStringLiteral("lang=%1 def=%2 valid=%3 len=%4")
                            .arg(language, definitionName.isEmpty()
                                               ? QStringLiteral("(none)")
                                               : definitionName)
                            .arg(valid)
                            .arg(code.size()));

    return QStringLiteral(
               "<code style=\"font-family:'IBM Plex Mono','Consolas',monospace;"
               "font-size:12px;"
               "background:%1;"
               "border:1px solid %2;"
               "border-radius:6px;"
               "padding:2px 6px;"
               "color:%3;"
               "white-space:pre;\">%4</code>")
        .arg(bg, border, textColor, html);
}

QString CodeHighlighter::renderMarkdown(const QString &markdown,
                                        bool dark) const
{
    QTextDocument document;
    document.setDocumentMargin(0);
    document.setMarkdown(renderInlineExtensionSyntax(markdown, this, dark));
    return document.toHtml();
}
