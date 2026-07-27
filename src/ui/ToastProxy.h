#pragma once

#include <QObject>

class ToastProxy : public QObject
{
    Q_OBJECT
  public:
    explicit ToastProxy(QObject *parent = nullptr);

    Q_INVOKABLE bool showMessage(const QString &message) const;
    Q_INVOKABLE bool forwardMessage(const QString &message);

  signals:
    void messageRequested(const QString &message);
};
