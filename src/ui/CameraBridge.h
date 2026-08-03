#pragma once

#include <QObject>

class CameraBridge : public QObject
{
    Q_OBJECT
  public:
    explicit CameraBridge(QObject *parent = nullptr);

    Q_INVOKABLE bool launchSystemCamera();

  signals:
    void captured(const QString &path);
    void errorOccurred(const QString &message);
    void permissionRequestLaunched(const QString &message);

  private:
#ifdef Q_OS_ANDROID
    bool ensureCameraPermission();
#endif
};
