#include "FilePicker.h"

#ifdef Q_OS_ANDROID
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/QMimeDatabase>
#include <QtCore/QStandardPaths>
#include <QtCore/QUuid>
#include <QtCore/private/qandroidextras_p.h>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qnativeinterface.h>

namespace
{
constexpr int kImagePickerRequestCode = 0x5342;
constexpr int kThemePickerRequestCode = 0x5343;

QString suffixForUri(const QJniObject &resolver, const QJniObject &uri)
{
    const QJniObject mime = resolver.callObjectMethod(
        "getType", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object());
    if (!mime.isValid())
        return QStringLiteral("bin");

    const QJniObject singleton = QJniObject::callStaticObjectMethod(
        "android/webkit/MimeTypeMap", "getSingleton",
        "()Landroid/webkit/MimeTypeMap;");
    if (!singleton.isValid())
        return QStringLiteral("bin");

    const QJniObject ext = singleton.callObjectMethod(
        "getExtensionFromMimeType", "(Ljava/lang/String;)Ljava/lang/String;",
        mime.object<jstring>());
    const QString suffix = ext.toString().trimmed().toLower();
    return suffix.isEmpty() ? QStringLiteral("bin") : suffix;
}

QString copyContentUriToTempFile(const QJniObject &resolver,
                                 const QJniObject &uri,
                                 const QString &forcedSuffix = QString())
{
    const QJniObject inputStream = resolver.callObjectMethod(
        "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;",
        uri.object());
    if (!inputStream.isValid())
        return QString();

    QString baseDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty())
        baseDir =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.isEmpty())
        return QString();

    QDir().mkpath(baseDir);
    const QString suffix = forcedSuffix.trimmed().isEmpty()
                               ? suffixForUri(resolver, uri)
                               : forcedSuffix.trimmed();
    const QString outPath =
        QDir(baseDir).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) +
                               QStringLiteral(".") + suffix);

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly))
    {
        inputStream.callMethod<void>("close");
        return QString();
    }

    QJniEnvironment env;
    jbyteArray buffer = env->NewByteArray(8192);
    if (!buffer)
    {
        inputStream.callMethod<void>("close");
        return QString();
    }

    while (true)
    {
        const jint read = inputStream.callMethod<jint>("read", "([B)I", buffer);
        if (read <= 0)
            break;

        jbyte *bytes = env->GetByteArrayElements(buffer, nullptr);
        if (!bytes)
        {
            env->DeleteLocalRef(buffer);
            inputStream.callMethod<void>("close");
            return QString();
        }
        outFile.write(reinterpret_cast<const char *>(bytes), read);
        env->ReleaseByteArrayElements(buffer, bytes, JNI_ABORT);
    }

    env->DeleteLocalRef(buffer);
    inputStream.callMethod<void>("close");
    outFile.close();
    return outPath;
}
} // namespace

QStringList FilePicker::pickImages()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
    {
        emit errorOccurred(QStringLiteral("Android context unavailable"));
        return {};
    }

    QJniObject action = QJniObject::fromString(
        QStringLiteral("android.intent.action.OPEN_DOCUMENT"));
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      action.object<jstring>());
    if (!intent.isValid())
    {
        emit errorOccurred(
            QStringLiteral("Failed to create image picker intent"));
        return {};
    }

    intent.callObjectMethod(
        "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(
            QStringLiteral("android.intent.category.OPENABLE"))
            .object<jstring>());
    intent.callObjectMethod(
        "setType", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(QStringLiteral("image/*")).object<jstring>());
    intent.callObjectMethod(
        "putExtra", "(Ljava/lang/String;Z)Landroid/content/Intent;",
        QJniObject::fromString(
            QStringLiteral("android.intent.extra.ALLOW_MULTIPLE"))
            .object<jstring>(),
        jboolean(true));

    QtAndroidPrivate::startActivity(
        intent, kImagePickerRequestCode,
        [this, context](int requestCode, int resultCode, const QJniObject &data)
        {
            if (requestCode != kImagePickerRequestCode)
                return;
            if (resultCode != -1 || !data.isValid())
            {
                emit errorOccurred(QStringLiteral("Image selection cancelled"));
                return;
            }

            const QJniObject resolver = context.callObjectMethod(
                "getContentResolver", "()Landroid/content/ContentResolver;");
            if (!resolver.isValid())
            {
                emit errorOccurred(
                    QStringLiteral("Content resolver unavailable"));
                return;
            }

            QStringList imported;

            const QJniObject clipData = data.callObjectMethod(
                "getClipData", "()Landroid/content/ClipData;");
            if (clipData.isValid())
            {
                const jint count = clipData.callMethod<jint>("getItemCount");
                for (jint i = 0; i < count; ++i)
                {
                    const QJniObject item = clipData.callObjectMethod(
                        "getItemAt", "(I)Landroid/content/ClipData$Item;", i);
                    if (!item.isValid())
                        continue;
                    const QJniObject uri =
                        item.callObjectMethod("getUri", "()Landroid/net/Uri;");
                    if (!uri.isValid())
                        continue;
                    const QString localPath =
                        copyContentUriToTempFile(resolver, uri);
                    if (!localPath.isEmpty())
                        imported.append(localPath);
                }
            }
            else
            {
                const QJniObject uri =
                    data.callObjectMethod("getData", "()Landroid/net/Uri;");
                if (uri.isValid())
                {
                    const QString localPath =
                        copyContentUriToTempFile(resolver, uri);
                    if (!localPath.isEmpty())
                        imported.append(localPath);
                }
            }

            if (imported.isEmpty())
            {
                emit errorOccurred(QStringLiteral("No images were selected"));
                return;
            }
            emit imagesPicked(imported);
        });

    return {};
}

QString FilePicker::pickThemePackage()
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
    {
        emit errorOccurred(QStringLiteral("Android context unavailable"));
        return {};
    }

    QJniObject action = QJniObject::fromString(
        QStringLiteral("android.intent.action.OPEN_DOCUMENT"));
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      action.object<jstring>());
    if (!intent.isValid())
    {
        emit errorOccurred(
            QStringLiteral("Failed to create theme package picker intent"));
        return {};
    }
    intent.callObjectMethod(
        "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(
            QStringLiteral("android.intent.category.OPENABLE"))
            .object<jstring>());
    intent.callObjectMethod(
        "setType", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(QStringLiteral("*/*")).object<jstring>());

    QtAndroidPrivate::startActivity(
        intent, kThemePickerRequestCode,
        [this, context](int requestCode, int resultCode, const QJniObject &data)
        {
            if (requestCode != kThemePickerRequestCode)
                return;
            if (resultCode != -1 || !data.isValid())
            {
                emit errorOccurred(
                    QStringLiteral("Theme package selection cancelled"));
                return;
            }
            const QJniObject resolver = context.callObjectMethod(
                "getContentResolver", "()Landroid/content/ContentResolver;");
            if (!resolver.isValid())
            {
                emit errorOccurred(
                    QStringLiteral("Content resolver unavailable"));
                return;
            }
            const QJniObject uri =
                data.callObjectMethod("getData", "()Landroid/net/Uri;");
            if (!uri.isValid())
            {
                emit errorOccurred(QStringLiteral("No theme package selected"));
                return;
            }
            const QString localPath =
                copyContentUriToTempFile(resolver, uri, QStringLiteral("tar.zst"));
            if (localPath.isEmpty())
            {
                emit errorOccurred(
                    QStringLiteral("Failed to import theme package"));
                return;
            }
            emit themePackagePicked(localPath);
        });

    return {};
}
#else
#include <QFileDialog>

QStringList FilePicker::pickImages()
{
    QFileDialog dialog(
        nullptr, QStringLiteral("Select Images"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles();
}

QString FilePicker::pickThemePackage()
{
    QFileDialog dialog(
        nullptr, QStringLiteral("Select Theme Package"), QString(),
        QStringLiteral("Theme packages (*.tar.zst *.tzst *.tar.gz *.tgz)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles().value(0);
}
#endif
