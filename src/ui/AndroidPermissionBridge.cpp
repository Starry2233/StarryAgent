#include "AndroidPermissionBridge.h"

#include "core/Config.h"

#include <QString>

#ifdef Q_OS_ANDROID
#include <QFuture>
#include <QtCore/QJniObject>
#include <QtCore/private/qandroidextras_p.h>
#include <QtCore/qnativeinterface.h>
#endif

AndroidPermissionBridge::AndroidPermissionBridge(Config *config, QObject *parent)
    : QObject(parent), m_config(config)
{
}

bool AndroidPermissionBridge::ensureRootAccessAndSetRoot(const QString &path)
{
    if (!m_config || path.isEmpty())
        return false;

#ifdef Q_OS_ANDROID
    if (!isAppPrivateRoot(path) && !ensureSharedStoragePermission())
        return false;
#endif

    if (!m_config->setRoot(path))
    {
        emit errorOccurred(QStringLiteral("Failed to switch root directory"));
        return false;
    }

    emit rootApplied(path);
    return true;
}

#ifdef Q_OS_ANDROID
bool AndroidPermissionBridge::isAppPrivateRoot(const QString &path) const
{
    const QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty() || !m_config)
        return false;

    return normalizedPath == m_config->defaultRoot()
           || normalizedPath.contains(QStringLiteral("/Android/data/"));
}

bool AndroidPermissionBridge::ensureSharedStoragePermission()
{
    if (QtAndroidPrivate::androidSdkVersion() >= 30)
        return ensureManageAllFilesAccess();

    const auto status = QtAndroidPrivate::checkPermission(
        QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE"));
    if (status.result() == QtAndroidPrivate::PermissionResult::Authorized)
        return true;

    const auto result = QtAndroidPrivate::requestPermission(
        QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE"));
    if (result.result() == QtAndroidPrivate::PermissionResult::Authorized)
    {
        emit permissionRequestLaunched(QStringLiteral(
            "Granted Android storage permission for the selected root."));
        return true;
    }

    emit errorOccurred(QStringLiteral(
        "Storage permission was denied. Choose an app-private root or grant storage access and try again."));
    return false;
}

bool AndroidPermissionBridge::ensureManageAllFilesAccess()
{
    const QJniObject environment("android/os/Environment");
    if (environment.isValid()
        && environment.callStaticMethod<jboolean>("android/os/Environment",
                                                  "isExternalStorageManager", "()Z"))
        return true;

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid())
    {
        emit errorOccurred(QStringLiteral(
            "Unable to request Android all-files access right now."));
        return false;
    }

    QJniObject action = QJniObject::fromString(
        QStringLiteral("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION"));
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      action.object<jstring>());
    if (!intent.isValid())
    {
        emit errorOccurred(QStringLiteral(
            "Unable to open Android all-files access settings."));
        return false;
    }

    const QString packageName = activity.callObjectMethod(
                                           "getPackageName", "()Ljava/lang/String;")
                                    .toString();
    const QJniObject uriString = QJniObject::fromString(
        QStringLiteral("package:%1").arg(packageName));
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        uriString.object<jstring>());
    if (uri.isValid())
        intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;",
                                uri.object());

    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", 0x10000000);
    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V",
                              intent.object());
    emit permissionRequestLaunched(QStringLiteral(
        "Allow Manage all files for StarryAgent, then retry this root."));
    return false;
}
#endif
