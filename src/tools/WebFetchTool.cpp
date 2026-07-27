#include "WebFetchTool.h"

#include <QtCore/qglobal.h>
#include <nlohmann/json.hpp>

#ifdef Q_OS_ANDROID
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#endif

using json = nlohmann::json;

QString WebFetchTool::description() const
{
    return QStringLiteral(
        "Fetch the content of a URL. Returns the first 2000 characters of the "
        "response body (plain text). Use for reading web pages, APIs, or text "
        "files.");
}

json WebFetchTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"url",
              {{"type", "string"}, {"description", "The URL to fetch."}}},
         }},
        {"required", {"url"}},
    };
}

#ifdef Q_OS_ANDROID
// On Android, use QNetworkAccessManager (backed by Android system TLS).
QString WebFetchTool::execute(const json &args)
{
    const std::string urlStr = args.value("url", std::string());
    if (urlStr.empty())
        return QStringLiteral("Error: `url` is required");

    QNetworkAccessManager mgr;
    QUrl url(QString::fromStdString(urlStr));

    QEventLoop loop;
    QNetworkReply *reply = mgr.get(QNetworkRequest(url));
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(15000); // 15s timeout
    loop.exec();

    if (!timer.isActive())
    {
        reply->abort();
        return QStringLiteral("Error: HTTP request timed out");
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        return QStringLiteral("Error: %1").arg(reply->errorString());
    }

    long httpCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toLongLong();
    QString body = QString::fromUtf8(reply->readAll());

    if (body.length() > 2000)
        body.truncate(2000);

    return QStringLiteral("HTTP ") + QString::number(httpCode) +
           QStringLiteral("\n\n") + body;
}
#else
// On desktop, use libcurl.
#include <curl/curl.h>

static size_t writeCb(void *ptr, size_t sz, size_t nm, void *ud)
{
    auto *out = static_cast<QString *>(ud);
    if (out->length() < 2000)
    {
        QString chunk =
            QString::fromUtf8(static_cast<const char *>(ptr), sz * nm);
        const int remaining = 2000 - out->length();
        if (chunk.length() > remaining)
            chunk.truncate(remaining);
        out->append(chunk);
    }
    return sz * nm;
}

QString WebFetchTool::execute(const json &args)
{
    const std::string url = args.value("url", std::string());
    if (url.empty())
        return QStringLiteral("Error: `url` is required");

    CURL *curl = curl_easy_init();
    if (!curl)
        return QStringLiteral("Error: curl_easy_init failed");

    QString result;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
        return QStringLiteral("Error: curl: %1").arg(curl_easy_strerror(rc));

    if (result.length() > 2000)
        result.truncate(2000);

    return QStringLiteral("HTTP ") + QString::number(httpCode) +
           QStringLiteral("\n\n") + result;
}
#endif
