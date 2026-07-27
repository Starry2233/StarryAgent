#pragma once

#include <QObject>

class ClipboardProxy : public QObject
{
    Q_OBJECT
  public:
    explicit ClipboardProxy(QObject *parent = nullptr);

    Q_INVOKABLE void setText(const QString &text);
    Q_INVOKABLE QString text() const;
    Q_INVOKABLE bool showCopyFeedback();
};
