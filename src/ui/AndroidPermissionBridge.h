#pragma once

#include <QObject>

class Config;

class AndroidPermissionBridge : public QObject
{
    Q_OBJECT
  public:
    explicit AndroidPermissionBridge(Config *config, QObject *parent = nullptr);

    Q_INVOKABLE bool ensureRootAccessAndSetRoot(const QString &path);

  signals:
    void rootApplied(const QString &path);
    void errorOccurred(const QString &message);
    void permissionRequestLaunched(const QString &message);

  private:
#ifdef Q_OS_ANDROID
    bool isAppPrivateRoot(const QString &path) const;
    bool ensureSharedStoragePermission();
    bool ensureManageAllFilesAccess();
#endif

    Config *m_config = nullptr;
};
