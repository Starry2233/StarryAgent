#pragma once

#include <QObject>

class CameraBridge : public QObject
{
    Q_OBJECT
  public:
    explicit CameraBridge(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE bool launchSystemCamera();

  signals:
    void captured(const QString &path);
    void errorOccurred(const QString &message);
};
