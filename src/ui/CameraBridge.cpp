#include "CameraBridge.h"

CameraBridge::CameraBridge(QObject *parent) : QObject(parent) {}

#ifdef Q_OS_ANDROID
#include <QFuture>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJniObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QUuid>
#include <QtCore/private/qandroidextras_p.h>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qnativeinterface.h>

namespace
{
constexpr int kCameraRequestCode = 0x5341;

class CameraActivityResultReceiver final : public QAndroidActivityResultReceiver
{
  public:
    explicit CameraActivityResultReceiver(CameraBridge *bridge)
        : m_bridge(bridge)
    {
    }

    void handleActivityResult(int receiverRequestCode, int resultCode,
                              const QJniObject &data) override
    {
        if (!m_bridge || receiverRequestCode != kCameraRequestCode)
            return;

        if (resultCode != -1)
        { // Activity.RESULT_OK
            QMetaObject::invokeMethod(
                m_bridge,
                [bridge = m_bridge]
                {
                    emit bridge->errorOccurred(
                        QStringLiteral("Camera capture cancelled"));
                },
                Qt::QueuedConnection);
            return;
        }

        const QJniObject extras =
            data.callObjectMethod("getExtras", "()Landroid/os/Bundle;");
        if (!extras.isValid())
        {
            QMetaObject::invokeMethod(
                m_bridge,
                [bridge = m_bridge]
                {
                    emit bridge->errorOccurred(
                        QStringLiteral("Camera did not return image data"));
                },
                Qt::QueuedConnection);
            return;
        }

        const QJniObject key = QJniObject::fromString(QStringLiteral("data"));
        const QJniObject bitmap = extras.callObjectMethod(
            "get", "(Ljava/lang/String;)Ljava/lang/Object;",
            key.object<jstring>());
        if (!bitmap.isValid())
        {
            QMetaObject::invokeMethod(
                m_bridge,
                [bridge = m_bridge]
                {
                    emit bridge->errorOccurred(
                        QStringLiteral("Camera thumbnail is unavailable"));
                },
                Qt::QueuedConnection);
            return;
        }

        QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (dir.isEmpty())
            dir =
                QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir().mkpath(dir);
        const QString path =
            dir + QLatin1Char('/') +
            QUuid::createUuid().toString(QUuid::WithoutBraces) +
            QStringLiteral(".png");

        const QJniObject javaPath = QJniObject::fromString(path);
        QJniObject outputStream("java/io/FileOutputStream",
                                "(Ljava/lang/String;)V",
                                javaPath.object<jstring>());
        if (!outputStream.isValid())
        {
            QMetaObject::invokeMethod(
                m_bridge,
                [bridge = m_bridge]
                {
                    emit bridge->errorOccurred(
                        QStringLiteral("Failed to create camera output file"));
                },
                Qt::QueuedConnection);
            return;
        }

        QJniObject compressFormat = QJniObject::getStaticObjectField(
            "android/graphics/Bitmap$CompressFormat", "PNG",
            "Landroid/graphics/Bitmap$CompressFormat;");
        const bool saved = bitmap.callMethod<jboolean>(
            "compress",
            "(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/"
            "OutputStream;)Z",
            compressFormat.object(), jint(100), outputStream.object());
        outputStream.callMethod<void>("close");

        if (!saved)
        {
            QFile::remove(path);
            QMetaObject::invokeMethod(
                m_bridge,
                [bridge = m_bridge]
                {
                    emit bridge->errorOccurred(
                        QStringLiteral("Failed to save camera image"));
                },
                Qt::QueuedConnection);
            return;
        }

        QMetaObject::invokeMethod(
            m_bridge, [bridge = m_bridge, path]
            { emit bridge->captured(path); }, Qt::QueuedConnection);
    }

  private:
    CameraBridge *m_bridge = nullptr;
};
} // namespace
#endif

#ifdef Q_OS_ANDROID
bool CameraBridge::ensureCameraPermission()
{
    const auto status = QtAndroidPrivate::checkPermission(
        QStringLiteral("android.permission.CAMERA"));
    if (status.result() == QtAndroidPrivate::PermissionResult::Authorized)
        return true;

    const auto result = QtAndroidPrivate::requestPermission(
        QStringLiteral("android.permission.CAMERA"));
    if (result.result() == QtAndroidPrivate::PermissionResult::Authorized)
    {
        emit permissionRequestLaunched(QStringLiteral(
            "Granted Android camera permission."));
        return true;
    }

    emit errorOccurred(QStringLiteral(
        "Camera permission was denied. Allow camera access and try again."));
    return false;
}
#endif

bool CameraBridge::launchSystemCamera()
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;
    if (!ensureCameraPermission())
        return false;

    QJniObject action = QJniObject::fromString(
        QStringLiteral("android.media.action.IMAGE_CAPTURE"));
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      action.object<jstring>());
    if (!intent.isValid())
        return false;

    auto *receiver = new CameraActivityResultReceiver(this);
    QtAndroidPrivate::startActivity(intent, kCameraRequestCode, receiver);
    return true;
#else
    return false;
#endif
}
