#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QTextDocument;

namespace KSyntaxHighlighting
{
class SyntaxHighlighter;
}

class CodeHighlighter : public QObject
{
    Q_OBJECT

  public:
    explicit CodeHighlighter(QObject *parent = nullptr);

    Q_INVOKABLE void applyToDocument(QObject *quickTextDocument,
                                     const QString &language, bool dark);
    Q_INVOKABLE QString inlineCodeHtml(const QString &code,
                                       const QString &language,
                                       bool dark) const;
    Q_INVOKABLE QString renderMarkdown(const QString &markdown,
                                       bool dark) const;

  private:
    QHash<QTextDocument *, QPointer<KSyntaxHighlighting::SyntaxHighlighter>>
        m_highlighters;
};
