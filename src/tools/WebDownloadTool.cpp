#include "WebDownloadTool.h"
#include "ToolWorkdir.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QUrl>

#include <nlohmann/json.hpp>

#ifdef Q_OS_ANDROID
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#else
#include <curl/curl.h>
#endif

using json = nlohmann::json;

QString WebDownloadTool::description() const
{
    return QStringLiteral(
        "Download a URL to a workspace-relative file path. "
        "Refuses absolute paths, "
        "path traversal, and overwriting existing files unless "
        "overwrite is true.");
}

json WebDownloadTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"url",
              {{"type", "string"}, {"description", "The URL to download."}}},
             {"path",
              {{"type", "string"},
               {"description", "Workspace-relative output path."}}},
             {"overwrite",
              {{"type", "boolean"},
               {"description", "Overwrite the file if it already exists."}}},
         }},
        {"required", {"url", "path"}},
    };
}

static QString resolveWorkspacePath(const QString &workspace,
                                    const QString &path)
{
    if (QDir::isAbsolutePath(path))
        return QString();

    const QString root = QDir(workspace).canonicalPath();
    if (root.isEmpty())
        return QString();

    const QString candidate = QDir::cleanPath(root + QDir::separator() + path);
    const QFileInfo info(candidate);
    const QString parent =
        QFileInfo(info.absolutePath()).exists()
            ? QDir(info.absolutePath()).canonicalPath()
            : QDir(QFileInfo(info.absolutePath()).absolutePath())
                  .canonicalPath();

    const QString normalizedRoot = QDir::cleanPath(root);
    const QString normalizedCandidate = QDir::cleanPath(candidate);
    if (normalizedCandidate == normalizedRoot ||
        !normalizedCandidate.startsWith(normalizedRoot + QStringLiteral("/")))
        return QString();
    if (!parent.isEmpty() && parent != normalizedRoot &&
        !parent.startsWith(normalizedRoot + QStringLiteral("/")))
        return QString();
    return normalizedCandidate;
}

#ifdef Q_OS_ANDROID
static QString downloadBytes(const QString &urlStr, QByteArray *out)
{
    QNetworkAccessManager mgr;
    QNetworkReply *reply = mgr.get(QNetworkRequest(QUrl(urlStr)));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(30000);
    loop.exec();

    if (!timer.isActive())
    {
        reply->abort();
        reply->deleteLater();
        return QStringLiteral("Error: HTTP request timed out");
    }
    if (reply->error() != QNetworkReply::NoError)
    {
        const QString err = reply->errorString();
        reply->deleteLater();
        return QStringLiteral("Error: %1").arg(err);
    }
    *out = reply->readAll();
    reply->deleteLater();
    return QString();
}
#else
static size_t writeBytesCb(void *ptr, size_t sz, size_t nm, void *ud)
{
    auto *out = static_cast<QByteArray *>(ud);
    out->append(static_cast<const char *>(ptr), qsizetype(sz * nm));
    return sz * nm;
}

static QString downloadBytes(const QString &urlStr, QByteArray *out)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return QStringLiteral("Error: curl_easy_init failed");

    const QByteArray url = urlStr.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, url.constData());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeBytesCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    const CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
        return QStringLiteral("Error: curl: %1").arg(curl_easy_strerror(rc));
    if (httpCode >= 400)
        return QStringLiteral("Error: HTTP %1").arg(httpCode);
    return QString();
}
#endif

QString WebDownloadTool::execute(const json &args)
{
    const std::string urlRaw = args.value("url", std::string());
    const std::string pathRaw = args.value("path", std::string());
    const bool overwrite = args.value("overwrite", false);
    if (urlRaw.empty())
        return QStringLiteral("Error: `url` is required");
    if (pathRaw.empty())
        return QStringLiteral("Error: `path` is required");
    const QString workdir = ToolWorkdir::effectiveWorkdir(args, m_workspace);

    const QString url = QString::fromStdString(urlRaw);
    if (!QUrl(url).isValid())
        return QStringLiteral("Error: invalid URL");

    const QString path =
        resolveWorkspacePath(workdir, QString::fromStdString(pathRaw));
    if (path.isEmpty())
        return QStringLiteral(
            "Error: `path` must be workspace-relative and must "
            "not escape the workspace");
    if (QFileInfo::exists(path) && !overwrite)
        return QStringLiteral(
                   "Error: file already exists: %1 (set overwrite=true "
                   "to replace it)")
            .arg(path);

    QByteArray bytes;
    const QString err = downloadBytes(url, &bytes);
    if (!err.isEmpty())
        return err;

    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QStringLiteral("Error: cannot create parent directory for %1")
            .arg(path);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QStringLiteral("Error: cannot open %1 for writing").arg(path);
    const qint64 written = f.write(bytes);
    f.close();
    if (written != bytes.size())
        return QStringLiteral("Error: short write to %1").arg(path);

    return QStringLiteral("downloaded %1 bytes to %2")
        .arg(bytes.size())
        .arg(path);
}
