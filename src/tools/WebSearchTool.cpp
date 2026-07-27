#include "WebSearchTool.h"

#include <QByteArray>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include <nlohmann/json.hpp>

#ifdef Q_OS_ANDROID
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#else
#include <curl/curl.h>
#endif

#include "core/Settings.h"

using json = nlohmann::json;

namespace
{

struct SearchHit
{
    QString title;
    QString url;
    QString snippet;
};

static QString stripTags(QString s)
{
    s.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    return s;
}

static QString decodeHtml(QString s)
{
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&ensp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&emsp;"), QStringLiteral(" "));

    QRegularExpression decEntity(QStringLiteral("&#(\\d+);"));
    QRegularExpressionMatch match;
    int offset = 0;
    while ((offset = s.indexOf(decEntity, offset, &match)) >= 0)
    {
        bool ok = false;
        const uint codepoint = match.captured(1).toUInt(&ok);
        const QString repl = ok ? QString(QChar(codepoint)) : QString();
        s.replace(offset, match.capturedLength(0), repl);
        offset += repl.size();
    }

    QRegularExpression hexEntity(QStringLiteral("&#x([0-9A-Fa-f]+);"));
    offset = 0;
    while ((offset = s.indexOf(hexEntity, offset, &match)) >= 0)
    {
        bool ok = false;
        const uint codepoint = match.captured(1).toUInt(&ok, 16);
        const QString repl = ok ? QString(QChar(codepoint)) : QString();
        s.replace(offset, match.capturedLength(0), repl);
        offset += repl.size();
    }
    return s.simplified();
}

static QString decodeHtmlText(const QString &html)
{
    return decodeHtml(stripTags(html));
}

static bool hostMatches(const QString &host, const QString &domain)
{
    const QString h = host.toLower();
    const QString d = domain.toLower();
    return h == d || h.endsWith(QStringLiteral(".") + d);
}

static QString resolveBingUrl(const QString &rawUrl)
{
    if (rawUrl.startsWith('/') || rawUrl.startsWith('#'))
        return QString();

    const QRegularExpression uParam(
        QStringLiteral("[?&]u=([A-Za-z0-9+/_=-]+)"));
    const QRegularExpressionMatch m = uParam.match(rawUrl);
    if (m.hasMatch())
    {
        const QString encoded = m.captured(1);
        if (encoded.size() >= 3)
        {
            QString b64 = encoded.mid(2);
            b64.replace('-', '+');
            b64.replace('_', '/');
            const QByteArray decoded = QByteArray::fromBase64(b64.toUtf8());
            const QString decodedUrl = QString::fromUtf8(decoded);
            if (decodedUrl.startsWith(QStringLiteral("http://")) ||
                decodedUrl.startsWith(QStringLiteral("https://")))
                return decodedUrl;
        }
    }

    if (!rawUrl.contains(QStringLiteral("bing.com"), Qt::CaseInsensitive))
        return rawUrl;
    return QString();
}

static bool passDomainFilters(const QString &urlStr,
                              const QSet<QString> &allowedDomains,
                              const QSet<QString> &blockedDomains)
{
    const QUrl url(urlStr);
    if (!url.isValid())
        return false;

    const QString host = url.host();
    if (host.isEmpty())
        return false;

    if (!allowedDomains.isEmpty())
    {
        bool allowed = false;
        for (const QString &d : allowedDomains)
        {
            if (hostMatches(host, d))
            {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            return false;
    }

    for (const QString &d : blockedDomains)
    {
        if (hostMatches(host, d))
            return false;
    }
    return true;
}

static QSet<QString> toDomainSet(const json &args, const char *key)
{
    QSet<QString> out;
    const auto it = args.find(key);
    if (it == args.end() || !it->is_array())
        return out;

    for (const auto &v : *it)
    {
        if (!v.is_string())
            continue;
        QString d =
            QString::fromStdString(v.get<std::string>()).trimmed().toLower();
        if (d.startsWith(QStringLiteral("www.")))
            d = d.mid(4);
        if (!d.isEmpty())
            out.insert(d);
    }
    return out;
}

static QList<SearchHit> extractCnBingResults(const QString &html)
{
    QList<SearchHit> out;

    const int olStart = html.indexOf(QStringLiteral("<ol id=\"b_results\""));
    if (olStart < 0)
        return out;
    const int olEnd = html.indexOf(QStringLiteral("</ol>"), olStart);
    if (olEnd < 0)
        return out;
    const QString resultsHtml = html.mid(olStart, olEnd - olStart);

    QRegularExpression liRe(
        QStringLiteral(
            "<li\\s+class=\"b_algo\"[^>]*>([\\s\\S]*?)(?=<li\\s+class=\"b_"
            "algo\"|"
            "<li\\s+class=\"b_msg\\s+b_canvas\"|<li\\s+class=\"b_pag\"|</ol>)"),
        QRegularExpression::CaseInsensitiveOption);
    auto it = liRe.globalMatch(resultsHtml);
    while (it.hasNext())
    {
        const QString block = it.next().captured(1);

        QRegularExpression titleRe(
            QStringLiteral(
                "<h2[^>]*>\\s*<a[^>]+href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch titleMatch = titleRe.match(block);
        if (!titleMatch.hasMatch())
            continue;

        QString url = decodeHtml(titleMatch.captured(1));
        url = resolveBingUrl(url);
        if (url.isEmpty())
            continue;

        SearchHit hit;
        hit.url = url;
        hit.title = decodeHtmlText(titleMatch.captured(2));

        QRegularExpression snipRe(
            QStringLiteral("<div[^>]*class=\"b_caption[^\\\"]*\"[^>]*>[\\s\\S]*"
                           "?<p[^>]*class="
                           "\"b_lineclamp[^\\\"]*\"[^>]*>([\\s\\S]*?)</p>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch snipMatch = snipRe.match(block);
        if (!snipMatch.hasMatch())
        {
            snipRe.setPattern(QStringLiteral(
                "<div[^>]*class=\"b_caption[^\\\"]*\"[^>]*>[\\s\\S]*?<"
                "p[^>]*>([\\s\\S]*?)</p>"));
            snipMatch = snipRe.match(block);
        }
        if (snipMatch.hasMatch())
            hit.snippet = decodeHtmlText(snipMatch.captured(1));

        if (!hit.title.isEmpty() && !hit.url.isEmpty())
            out.append(hit);
    }
    return out;
}

static QList<SearchHit> extractGlobalBingResults(const QString &html)
{
    QList<SearchHit> out;
    QRegularExpression liRe(
        QStringLiteral(
            "<li\\s+class=\"b_algo\"[^>]*>([\\s\\S]*?)(?=<li\\s+class="
            "\"b_algo\"|<li\\s+class=\"b_pag\"|</ol>)"),
        QRegularExpression::CaseInsensitiveOption);
    auto it = liRe.globalMatch(html);
    while (it.hasNext())
    {
        const QString block = it.next().captured(1);

        QRegularExpression titleRe(
            QStringLiteral(
                "<h2[^>]*>\\s*<a[^>]+href=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</a>"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch titleMatch = titleRe.match(block);
        if (!titleMatch.hasMatch())
            continue;

        QString url = decodeHtml(titleMatch.captured(1));
        url = resolveBingUrl(url);
        if (url.isEmpty())
            continue;

        SearchHit hit;
        hit.url = url;
        hit.title = decodeHtmlText(titleMatch.captured(2));

        QRegularExpression snipRe(
            QStringLiteral(
                "<p[^>]*class=\"b_lineclamp[^\\\"]*\"[^>]*>([\\s\\S]*?)</p>"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch snipMatch = snipRe.match(block);
        if (!snipMatch.hasMatch())
        {
            snipRe.setPattern(QStringLiteral(
                "<div[^>]*class=\"b_caption[^\\\"]*\"[^>]*>[\\s\\S]*?<"
                "p[^>]*>([\\s\\S]*?)</p>"));
            snipMatch = snipRe.match(block);
        }
        if (snipMatch.hasMatch())
            hit.snippet = decodeHtmlText(snipMatch.captured(1));

        if (!hit.title.isEmpty() && !hit.url.isEmpty())
            out.append(hit);
    }
    return out;
}

struct HttpResponse
{
    long statusCode = 0;
    QString body;
    QString headers;
    QString error;
};

#ifdef Q_OS_ANDROID
static HttpResponse httpGet(const QString &urlStr,
                            const QList<QPair<QByteArray, QByteArray>> &headers,
                            int timeoutMs = 30000)
{
    HttpResponse out;
    QNetworkAccessManager mgr;
    QNetworkRequest req{QUrl(urlStr)};
    for (const auto &h : headers)
        req.setRawHeader(h.first, h.second);

    QNetworkReply *reply = mgr.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!timer.isActive())
    {
        reply->abort();
        out.error = QStringLiteral("Error: HTTP request timed out");
        reply->deleteLater();
        return out;
    }

    out.statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toLongLong();
    const auto rawHeaders = reply->rawHeaderPairs();
    for (const auto &h : rawHeaders)
        out.headers += QString::fromUtf8(h.first) + QStringLiteral(": ") +
                       QString::fromUtf8(h.second) + QLatin1Char('\n');
    out.body = QString::fromUtf8(reply->readAll());
    if (reply->error() != QNetworkReply::NoError)
        out.error = QStringLiteral("Error: %1").arg(reply->errorString());
    reply->deleteLater();
    return out;
}

static HttpResponse httpPostJson(
    const QString &urlStr, const QByteArray &body,
    const QList<QPair<QByteArray, QByteArray>> &headers, int timeoutMs = 30000)
{
    HttpResponse out;
    QNetworkAccessManager mgr;
    QNetworkRequest req{QUrl(urlStr)};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    for (const auto &h : headers)
        req.setRawHeader(h.first, h.second);

    QNetworkReply *reply = mgr.post(req, body);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!timer.isActive())
    {
        reply->abort();
        out.error = QStringLiteral("Error: HTTP request timed out");
        reply->deleteLater();
        return out;
    }

    out.statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toLongLong();
    const auto rawHeaders = reply->rawHeaderPairs();
    for (const auto &h : rawHeaders)
        out.headers += QString::fromUtf8(h.first) + QStringLiteral(": ") +
                       QString::fromUtf8(h.second) + QLatin1Char('\n');
    out.body = QString::fromUtf8(reply->readAll());
    if (reply->error() != QNetworkReply::NoError)
        out.error = QStringLiteral("Error: %1").arg(reply->errorString());
    reply->deleteLater();
    return out;
}
#else
static size_t writeToQStringCb(void *ptr, size_t sz, size_t nm, void *ud)
{
    auto *out = static_cast<QString *>(ud);
    out->append(
        QString::fromUtf8(static_cast<const char *>(ptr), int(sz * nm)));
    return sz * nm;
}

static size_t writeHeaderToQStringCb(char *ptr, size_t sz, size_t nm, void *ud)
{
    auto *out = static_cast<QString *>(ud);
    out->append(QString::fromUtf8(ptr, int(sz * nm)));
    return sz * nm;
}

static HttpResponse httpGet(const QString &urlStr,
                            const QList<QPair<QByteArray, QByteArray>> &headers,
                            int timeoutMs = 30000)
{
    HttpResponse out;
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        out.error = QStringLiteral("Error: curl_easy_init failed");
        return out;
    }

    struct curl_slist *hdrs = nullptr;
    for (const auto &h : headers)
    {
        const QByteArray line = h.first + ": " + h.second;
        hdrs = curl_slist_append(hdrs, line.constData());
    }

    const QByteArray url = urlStr.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, url.constData());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToQStringCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &writeHeaderToQStringCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &out.headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, long(timeoutMs));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, long(timeoutMs));

    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.statusCode);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
        out.error =
            QStringLiteral("Error: curl: %1").arg(curl_easy_strerror(rc));
    return out;
}

static HttpResponse httpPostJson(
    const QString &urlStr, const QByteArray &body,
    const QList<QPair<QByteArray, QByteArray>> &headers, int timeoutMs = 30000)
{
    HttpResponse out;
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        out.error = QStringLiteral("Error: curl_easy_init failed");
        return out;
    }

    struct curl_slist *hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    for (const auto &h : headers)
    {
        const QByteArray line = h.first + ": " + h.second;
        hdrs = curl_slist_append(hdrs, line.constData());
    }

    const QByteArray url = urlStr.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, url.constData());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.constData());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, long(body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToQStringCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &writeHeaderToQStringCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &out.headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, long(timeoutMs));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, long(timeoutMs));

    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.statusCode);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
        out.error =
            QStringLiteral("Error: curl: %1").arg(curl_easy_strerror(rc));
    return out;
}
#endif

static QList<QPair<QByteArray, QByteArray>> browserHeaders(bool chinaBing)
{
    QList<QPair<QByteArray, QByteArray>> headers = {
        {"User-Agent",
         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, "
         "like Gecko) Chrome/131.0.0.0 Safari/537.36 Edg/131.0.0.0"},
        {"Accept",
         "text/html,application/xhtml+xml,application/xml;q=0.9,image/"
         "avif,image/webp,image/apng,*/*;q=0.8"},
        {"Accept-Language",
         chinaBing ? "zh-CN,zh;q=0.9,en;q=0.8" : "en-US,en;q=0.9"},
        {"Cache-Control", "no-cache"},
        {"Pragma", "no-cache"},
        {"Upgrade-Insecure-Requests", "1"}};
    return headers;
}

static QList<QPair<QByteArray, QByteArray>>
apiHeaders(const QString &apiKey)
{
    QList<QPair<QByteArray, QByteArray>> headers = {
        {"Accept", "application/json"},
        {"User-Agent",
         "StarryAgent/0.1 (+https://local.starryagent.invalid)"}};
    if (!apiKey.trimmed().isEmpty())
    {
        headers.append({"Authorization",
                        QByteArray("Bearer ") + apiKey.trimmed().toUtf8()});
    }
    return headers;
}

static bool shouldUseChinaBing()
{
    static bool initialized = false;
    static bool useChina = false;
    if (initialized)
        return useChina;
    initialized = true;

    const HttpResponse resp =
        httpGet(QStringLiteral("https://ipinfo.io/json"), {}, 10000);
    if (resp.error.isEmpty() && resp.statusCode >= 200 && resp.statusCode < 300)
    {
        const QJsonDocument doc = QJsonDocument::fromJson(resp.body.toUtf8());
        const QString country = doc.object()
                                    .value(QStringLiteral("country"))
                                    .toString()
                                    .trimmed()
                                    .toUpper();
        useChina = (country == QStringLiteral("CN"));
    }
    return useChina;
}

static QString renderResults(const QString &query, const QList<SearchHit> &hits,
                             bool chinaBing, bool someResultsHidden)
{
    QString out =
        QStringLiteral("Web search results for query: \"%1\"\n\n").arg(query);
    if (hits.isEmpty())
    {
        out += QStringLiteral("No search results found.");
    }
    else
    {
        out += QStringLiteral("Links:\n");
        for (const SearchHit &hit : hits)
        {
            out += QStringLiteral("- [%1](%2)").arg(hit.title, hit.url);
            if (!hit.snippet.isEmpty())
                out += QStringLiteral(": %1").arg(hit.snippet);
            out += QLatin1Char('\n');
        }
    }

    if (chinaBing && someResultsHidden)
        out += QStringLiteral(
            "\nNote: cn.bing.com indicates that some results may "
            "be hidden due to local legal requirements.");
    out += QStringLiteral("\n\nREMINDER: You MUST include the sources above in "
                          "your response using markdown hyperlinks.");
    return out.trimmed();
}

static QString renderApiResults(const QString &query, const QString &output,
                                const QJsonArray &results)
{
    QStringList lines;
    if (!output.trimmed().isEmpty())
        lines << output.trimmed();
    else
        lines << QStringLiteral("Web search completed for query: \"%1\".")
                     .arg(query);

    QStringList links;
    for (const QJsonValue &value : results)
    {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
        if (url.isEmpty())
            continue;
        QString title = obj.value(QStringLiteral("title")).toString().trimmed();
        if (title.isEmpty())
            title = obj.value(QStringLiteral("name")).toString().trimmed();
        if (title.isEmpty())
            title = url;
        QString line = QStringLiteral("- [%1](%2)").arg(title, url);
        const QString snippet =
            obj.value(QStringLiteral("snippet")).toString().trimmed();
        if (!snippet.isEmpty())
            line += QStringLiteral(": %1").arg(snippet);
        links << line;
    }
    if (!links.isEmpty())
        lines << QStringLiteral("Sources:\n%1").arg(links.join(QLatin1Char('\n')));
    lines << QStringLiteral(
        "REMINDER: You MUST include the sources above in your response using markdown hyperlinks.");
    return lines.join(QStringLiteral("\n\n")).trimmed();
}

static QString buildSearchEndpoint(QString baseUrl)
{
    baseUrl = baseUrl.trimmed();
    if (baseUrl.isEmpty())
        return QString();
    while (baseUrl.endsWith(QLatin1Char('/')))
        baseUrl.chop(1);
    if (baseUrl.endsWith(QStringLiteral("/v1")))
        return baseUrl + QStringLiteral("/alpha/search");
    if (baseUrl.endsWith(QStringLiteral("/alpha/search")))
        return baseUrl;
    if (baseUrl.contains(QStringLiteral("/v1/")))
        return baseUrl + QStringLiteral("/alpha/search");
    return baseUrl + QStringLiteral("/v1/alpha/search");
}

static QString executeApiSearch(const QString &query,
                                const QSet<QString> &allowedDomains,
                                const QSet<QString> &blockedDomains, int limit,
                                const QString &baseUrl, const QString &apiKey,
                                const QString &model)
{
    if (baseUrl.trimmed().isEmpty())
        return QStringLiteral("Error: API search base URL is empty.");
    if (apiKey.trimmed().isEmpty())
        return QStringLiteral("Error: API search API key is empty.");
    if (model.trimmed().isEmpty())
        return QStringLiteral("Error: API search model is empty.");

    QJsonObject queryObj{{QStringLiteral("q"), query}};
    QJsonArray searchQueries;
    searchQueries.append(queryObj);

    QJsonObject commands{
        {QStringLiteral("search_query"), searchQueries},
        {QStringLiteral("response_length"), QStringLiteral("short")}};
    QJsonObject filters;
    if (!allowedDomains.isEmpty())
    {
        QJsonArray domains;
        for (const QString &domain : allowedDomains)
            domains.append(domain);
        filters.insert(QStringLiteral("allowed_domains"), domains);
    }
    if (!blockedDomains.isEmpty())
    {
        QJsonArray domains;
        for (const QString &domain : blockedDomains)
            domains.append(domain);
        filters.insert(QStringLiteral("blocked_domains"), domains);
    }

    QJsonObject settings{
        {QStringLiteral("search_context_size"), QStringLiteral("medium")},
        {QStringLiteral("external_web_access"), true}};
    if (!filters.isEmpty())
        settings.insert(QStringLiteral("filters"), filters);

    QJsonObject body{
        {QStringLiteral("id"),
         QStringLiteral("starryagent-web-search-%1")
             .arg(QDateTime::currentMSecsSinceEpoch())},
        {QStringLiteral("model"), model},
        {QStringLiteral("commands"), commands},
        {QStringLiteral("settings"), settings},
        {QStringLiteral("max_output_tokens"), 1800}};

    const HttpResponse resp = httpPostJson(
        buildSearchEndpoint(baseUrl),
        QJsonDocument(body).toJson(QJsonDocument::Compact), apiHeaders(apiKey),
        120000);
    if (!resp.error.isEmpty())
        return QStringLiteral("%1\n\nResponse headers:\n%2\nResponse body:\n%3")
            .arg(resp.error, resp.headers.trimmed(), resp.body);
    if (resp.statusCode >= 400)
        return QStringLiteral("Error: HTTP %1\n\nResponse headers:\n%2\n"
                              "Response body:\n%3")
            .arg(resp.statusCode)
            .arg(resp.headers.trimmed(), resp.body);
    if (resp.body.trimmed().isEmpty())
        return QStringLiteral("Error: empty search response");

    const QJsonDocument doc = QJsonDocument::fromJson(resp.body.toUtf8());
    if (!doc.isObject())
        return QStringLiteral("Error: invalid JSON search response\n\n%1")
            .arg(resp.body);
    const QJsonObject obj = doc.object();
    const QString output = obj.value(QStringLiteral("output")).toString();
    const QJsonArray results = obj.value(QStringLiteral("results")).toArray();

    QList<SearchHit> filtered;
    for (const QJsonValue &value : results)
    {
        if (!value.isObject())
            continue;
        const QJsonObject item = value.toObject();
        SearchHit hit;
        hit.title = item.value(QStringLiteral("title")).toString().trimmed();
        if (hit.title.isEmpty())
            hit.title = item.value(QStringLiteral("name")).toString().trimmed();
        hit.url = item.value(QStringLiteral("url")).toString().trimmed();
        hit.snippet = item.value(QStringLiteral("snippet")).toString().trimmed();
        if (hit.url.isEmpty() ||
            !passDomainFilters(hit.url, allowedDomains, blockedDomains))
            continue;
        filtered.append(hit);
        if (filtered.size() >= limit)
            break;
    }

    QJsonArray filteredResults;
    for (const SearchHit &hit : filtered)
    {
        filteredResults.append(QJsonObject{
            {QStringLiteral("title"), hit.title},
            {QStringLiteral("url"), hit.url},
            {QStringLiteral("snippet"), hit.snippet}});
    }
    return renderApiResults(query, output, filteredResults);
}

static QString executeAliyunDashScopeInternalSearch(
    const QString &query, const QSet<QString> &allowedDomains,
    const QSet<QString> &blockedDomains, const QString &baseUrl,
    const QString &apiKey, int limit)
{
    if (!allowedDomains.isEmpty() || !blockedDomains.isEmpty())
        return QStringLiteral("Error: Aliyun DashScope internal search does "
                              "not support `allowed_domains` or "
                              "`blocked_domains`.");

    const QString resolvedBaseUrl =
        baseUrl.trimmed().isEmpty()
            ? QStringLiteral("https://xxxx-hangzhou.opensearch.aliyuncs.com")
            : baseUrl.trimmed();
    if (apiKey.trimmed().isEmpty())
        return QStringLiteral("Error: DashScope API key is empty.");

    QString endpoint = resolvedBaseUrl;
    while (endpoint.endsWith(QLatin1Char('/')))
        endpoint.chop(1);
    if (!endpoint.contains(QStringLiteral("/v3/openapi/workspaces/")))
    {
        endpoint += QStringLiteral(
            "/v3/openapi/workspaces/default/web-search/ops-web-search-001");
    }
    QJsonObject body{{QStringLiteral("query"), query},
                     {QStringLiteral("query_rewrite"), true},
                     {QStringLiteral("top_k"), qBound(1, limit, 10)},
                     {QStringLiteral("content_type"),
                      QStringLiteral("snippet")}};

    const HttpResponse resp = httpPostJson(
        endpoint, QJsonDocument(body).toJson(QJsonDocument::Compact),
        apiHeaders(apiKey), 120000);
    if (!resp.error.isEmpty())
        return QStringLiteral("%1\n\nResponse headers:\n%2\nResponse body:\n%3")
            .arg(resp.error, resp.headers.trimmed(), resp.body);
    if (resp.statusCode >= 400)
        return QStringLiteral("Error: HTTP %1\n\nResponse headers:\n%2\n"
                              "Response body:\n%3")
            .arg(resp.statusCode)
            .arg(resp.headers.trimmed(), resp.body);
    if (resp.body.trimmed().isEmpty())
        return QStringLiteral("Error: empty DashScope search response");

    const QJsonDocument doc = QJsonDocument::fromJson(resp.body.toUtf8());
    if (!doc.isObject())
        return QStringLiteral("Error: invalid JSON DashScope response\n\n%1")
            .arg(resp.body);
    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("code")))
    {
        return QStringLiteral("Error: %1 (%2)\n\nResponse body:\n%3")
            .arg(obj.value(QStringLiteral("message")).toString().trimmed(),
                 obj.value(QStringLiteral("code")).toString().trimmed(),
                 resp.body);
    }

    const QJsonObject result = obj.value(QStringLiteral("result")).toObject();
    const QJsonArray searchResults =
        result.value(QStringLiteral("search_result")).toArray();
    if (searchResults.isEmpty())
        return QStringLiteral("No search results found.");

    QString out =
        QStringLiteral("Web search results for query: \"%1\"\n\nLinks:\n")
            .arg(query);
    int refIndex = 1;
    for (const QJsonValue &value : searchResults)
    {
        if (!value.isObject())
            continue;
        const QJsonObject item = value.toObject();
        const QString title = item.value(QStringLiteral("title")).toString();
        const QString link = item.value(QStringLiteral("link")).toString();
        const QString snippet =
            item.value(QStringLiteral("snippet")).toString().trimmed();
        if (title.trimmed().isEmpty() || link.trimmed().isEmpty())
            continue;
        out += QStringLiteral("- [%1](%2)").arg(title, link);
        if (!snippet.isEmpty())
            out += QStringLiteral(": %1").arg(snippet);
        out += QStringLiteral(" [ref_%1]\n").arg(refIndex++);
    }
    out += QStringLiteral("\nREMINDER: You MUST include the sources above in "
                          "your response using markdown hyperlinks.");
    return out.trimmed();
}

} // namespace

QString WebSearchTool::description() const
{
    return QStringLiteral(
        "Search the web for up-to-date information. Implementation is "
        "selected in Settings: Bing Search (Legacy), the current API search "
        "interface, Aliyun DashScope internal search, or an external "
        "API-compatible search endpoint.");
}

json WebSearchTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"query",
              {{"type", "string"}, {"description", "The search query."}}},
             {"limit",
              {{"type", "integer"},
               {"description", "Maximum number of results to return."},
               {"minimum", 1},
               {"maximum", 10}}},
             {"allowed_domains",
              {{"type", "array"},
               {"description", "Only include results from these domains."},
               {"items", {{"type", "string"}}}}},
             {"blocked_domains",
              {{"type", "array"},
               {"description", "Exclude results from these domains."},
               {"items", {{"type", "string"}}}}},
         }},
        {"required", {"query"}},
    };
}

QString WebSearchTool::execute(const json &args)
{
    const std::string query = args.value("query", std::string());
    if (query.empty())
        return QStringLiteral("Error: `query` is required");

    const QSet<QString> allowedDomains = toDomainSet(args, "allowed_domains");
    const QSet<QString> blockedDomains = toDomainSet(args, "blocked_domains");
    if (!allowedDomains.isEmpty() && !blockedDomains.isEmpty())
        return QStringLiteral(
            "Error: cannot specify both `allowed_domains` and "
            "`blocked_domains` in the same request");

    int limit = args.value("limit", 5);
    if (limit < 1)
        limit = 1;
    if (limit > 10)
        limit = 10;

    const QString q = QString::fromStdString(query).trimmed();
    const QString implementation = m_settings
                                       ? m_settings->webSearchImplementation()
                                       : QStringLiteral("bing_legacy");

    if (implementation == QStringLiteral("current_api"))
    {
        return executeApiSearch(
            q, allowedDomains, blockedDomains, limit,
            m_settings ? m_settings->apiBaseUrl() : QString(),
            m_settings ? m_settings->apiKey() : QString(),
            m_settings ? m_settings->webSearchModel() : QString());
    }
    if (implementation == QStringLiteral("aliyun_dashscope_internal"))
    {
        return executeAliyunDashScopeInternalSearch(
            q, allowedDomains, blockedDomains,
            m_settings ? m_settings->webSearchExternalBaseUrl() : QString(),
            m_settings ? m_settings->webSearchExternalApiKey() : QString(),
            limit);
    }
    if (implementation == QStringLiteral("external_api"))
    {
        return executeApiSearch(
            q, allowedDomains, blockedDomains, limit,
            m_settings ? m_settings->webSearchExternalBaseUrl() : QString(),
            m_settings ? m_settings->webSearchExternalApiKey() : QString(),
            m_settings ? m_settings->webSearchModel() : QString());
    }

    const bool chinaBing = shouldUseChinaBing();

    QUrl url(chinaBing ? QStringLiteral("https://cn.bing.com/search")
                       : QStringLiteral("https://www.bing.com/search"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("q"), q);
    if (chinaBing)
    {
        urlQuery.addQueryItem(QStringLiteral("form"), QStringLiteral("QBLHCN"));
        urlQuery.addQueryItem(QStringLiteral("ensearch"), QStringLiteral("1"));
    }
    else
        urlQuery.addQueryItem(QStringLiteral("setmkt"),
                              QStringLiteral("en-US"));
    url.setQuery(urlQuery);

    const HttpResponse resp = httpGet(QString::fromUtf8(url.toEncoded()),
                                      browserHeaders(chinaBing), 120000);
    if (!resp.error.isEmpty())
        return QStringLiteral("%1\n\nResponse headers:\n%2\nResponse body:\n%3")
            .arg(resp.error, resp.headers.trimmed(), resp.body);
    if (resp.statusCode >= 400)
        return QStringLiteral("Error: HTTP %1\n\nResponse "
                              "headers:\n%2\nResponse body:\n%3")
            .arg(resp.statusCode)
            .arg(resp.headers.trimmed(), resp.body);
    if (resp.body.isEmpty())
        return QStringLiteral("Error: empty search response");

    const bool someResultsHidden =
        resp.body.contains(QStringLiteral("部分搜索结果未予显示")) ||
        resp.body.contains(QStringLiteral("一些您可能无法访问的结果已被隐去"));

    QList<SearchHit> hits = chinaBing ? extractCnBingResults(resp.body)
                                      : extractGlobalBingResults(resp.body);

    QList<SearchHit> filtered;
    for (const SearchHit &hit : hits)
    {
        if (passDomainFilters(hit.url, allowedDomains, blockedDomains))
            filtered.append(hit);
        if (filtered.size() >= limit)
            break;
    }

    return renderResults(q, filtered, chinaBing, someResultsHidden);
}
