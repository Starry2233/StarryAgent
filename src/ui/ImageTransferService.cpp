#include "ImageTransferService.h"

#include "ToastService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace
{
void notifyDownloadResult(const QString &savedPath, const QString &error)
{
    if (!error.isEmpty())
        ToastService::showMessage(QStringLiteral("下载失败: %1").arg(error));
    else if (!savedPath.isEmpty())
        ToastService::showMessage(QStringLiteral("已保存到 %1").arg(savedPath));
}

QString sanitizeName(QString name)
{
    name = name.trimmed();
    if (name.isEmpty())
        name = QStringLiteral("image");

    static const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (QChar &ch : name)
    {
        if (illegal.contains(ch) || ch.isSpace())
            ch = QLatin1Char('_');
    }
    return name;
}

QString downloadDir()
{
    QString dir =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty())
        dir =
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty())
        dir =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return dir;
}

QString uniqueFilePath(const QString &directory, const QString &baseName,
                       const QString &suffix)
{
    QDir dir(directory);
    QString candidate = dir.filePath(baseName + suffix);
    if (!QFileInfo::exists(candidate))
        return candidate;

    for (int i = 2; i < 10000; ++i)
    {
        candidate = dir.filePath(
            QStringLiteral("%1-%2%3").arg(baseName).arg(i).arg(suffix));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return dir.filePath(baseName + QStringLiteral("-overflow") + suffix);
}

QString destinationPathFor(const QString &source, const QString &suggestedName,
                           const QString &mimeName = QString())
{
    const QString dir = downloadDir();
    QDir().mkpath(dir);

    QString baseName = sanitizeName(suggestedName);
    QString suffix;

    const QUrl url = QUrl::fromUserInput(source);
    if (baseName == QStringLiteral("image"))
    {
        if (url.isLocalFile())
        {
            const QFileInfo info(url.toLocalFile());
            if (!info.completeBaseName().isEmpty())
                baseName = sanitizeName(info.completeBaseName());
            suffix = info.suffix().isEmpty()
                         ? QString()
                         : QStringLiteral(".") + info.suffix();
        }
        else
        {
            const QFileInfo info(url.path());
            if (!info.completeBaseName().isEmpty())
                baseName = sanitizeName(info.completeBaseName());
            suffix = info.suffix().isEmpty()
                         ? QString()
                         : QStringLiteral(".") + info.suffix();
        }
    }

    if (suffix.isEmpty() && !mimeName.isEmpty())
    {
        const QString preferred =
            QMimeDatabase().mimeTypeForName(mimeName).preferredSuffix();
        if (!preferred.isEmpty())
            suffix = QStringLiteral(".") + preferred;
    }
    if (suffix.isEmpty())
        suffix = QStringLiteral(".png");

    return uniqueFilePath(dir, baseName, suffix);
}
} // namespace

ImageTransferService::ImageTransferService(QObject *parent) : QObject(parent) {}

void ImageTransferService::download(const QString &source,
                                    const QString &suggestedName)
{
    if (source.trimmed().isEmpty())
    {
        notifyDownloadResult(QString(), QStringLiteral("Empty image source"));
        emit downloadFinished(source, QString(),
                              QStringLiteral("Empty image source"));
        return;
    }

    const QUrl url = QUrl::fromUserInput(source);
    if (url.isLocalFile())
    {
        const QString localPath = url.toLocalFile();
        if (!QFileInfo::exists(localPath))
        {
            notifyDownloadResult(QString(),
                                 QStringLiteral("Image file not found"));
            emit downloadFinished(source, QString(),
                                  QStringLiteral("Image file not found"));
            return;
        }

        const QString destination = destinationPathFor(source, suggestedName);
        if (!QFile::copy(localPath, destination))
        {
            notifyDownloadResult(QString(),
                                 QStringLiteral("Failed to save image"));
            emit downloadFinished(source, QString(),
                                  QStringLiteral("Failed to save image"));
            return;
        }
        notifyDownloadResult(destination, QString());
        emit downloadFinished(source, destination, QString());
        return;
    }

    if (source.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive))
    {
        const int comma = source.indexOf(QLatin1Char(','));
        if (comma < 0)
        {
            notifyDownloadResult(QString(), QStringLiteral("Invalid data URL"));
            emit downloadFinished(source, QString(),
                                  QStringLiteral("Invalid data URL"));
            return;
        }

        const QString header = source.left(comma);
        const QString payload = source.mid(comma + 1);
        const QString mimeName =
            header.mid(5).section(QLatin1Char(';'), 0, 0).trimmed();
        const QByteArray bytes = QByteArray::fromBase64(payload.toUtf8());
        if (bytes.isEmpty())
        {
            notifyDownloadResult(QString(),
                                 QStringLiteral("Invalid image data"));
            emit downloadFinished(source, QString(),
                                  QStringLiteral("Invalid image data"));
            return;
        }

        const QString destination =
            destinationPathFor(source, suggestedName, mimeName);
        QFile file(destination);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(bytes) != bytes.size())
        {
            notifyDownloadResult(QString(),
                                 QStringLiteral("Failed to save image"));
            emit downloadFinished(source, QString(),
                                  QStringLiteral("Failed to save image"));
            return;
        }
        notifyDownloadResult(destination, QString());
        emit downloadFinished(source, destination, QString());
        return;
    }

    QNetworkReply *reply = m_network.get(QNetworkRequest(url));
    connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, source, suggestedName]
        {
            const QByteArray bytes = reply->readAll();
            const QString error = reply->error() == QNetworkReply::NoError
                                      ? QString()
                                      : reply->errorString();
            const QString mimeName =
                reply->header(QNetworkRequest::ContentTypeHeader).toString();
            reply->deleteLater();

            if (!error.isEmpty())
            {
                notifyDownloadResult(QString(), error);
                emit downloadFinished(source, QString(), error);
                return;
            }

            const QString destination =
                destinationPathFor(source, suggestedName, mimeName);
            QFile file(destination);
            if (!file.open(QIODevice::WriteOnly) ||
                file.write(bytes) != bytes.size())
            {
                notifyDownloadResult(QString(),
                                     QStringLiteral("Failed to save image"));
                emit downloadFinished(source, QString(),
                                      QStringLiteral("Failed to save image"));
                return;
            }

            notifyDownloadResult(destination, QString());
            emit downloadFinished(source, destination, QString());
        });
}
