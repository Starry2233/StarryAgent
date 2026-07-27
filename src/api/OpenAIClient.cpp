#include "OpenAIClient.h"

#include <QJsonDocument>

#include <nlohmann/json.hpp>

#include "core/DebugTrace.h"

using json = nlohmann::json;

namespace
{
QStringList chunkLines(const QString &text)
{
    return text.split(QLatin1Char('\n'));
}

void logVerboseMultiline(const char *module, const QString &header,
                        const QString &body)
{
    DebugTrace::verbose(module, header);
    const QStringList lines = chunkLines(body);
    if (lines.isEmpty())
    {
        DebugTrace::verbose(module, QStringLiteral("  <empty>"));
        return;
    }
    for (const QString &line : lines)
        DebugTrace::verbose(module, QStringLiteral("  %1").arg(line));
}
}

OpenAIClient::OpenAIClient(QObject *parent) : QObject(parent)
{
    wirePipeline();
}

OpenAIClient::~OpenAIClient()
{
    m_cancel = true;
    if (m_worker.joinable())
        m_worker.join();
}

void OpenAIClient::wirePipeline()
{
    m_sse.onData = [this](const std::string &data)
    { m_assembler.onData(data); };
    m_sse.onDone = [this] { m_assembler.onStreamEnd(); };

    m_assembler.onContentDelta = [this](const std::string &text)
    { emit contentDelta(QString::fromStdString(text)); };
    m_assembler.onToolCallStart =
        [this](int, const std::string &id, const std::string &name)
    {
        emit toolCallComposing(QString::fromStdString(id),
                               QString::fromStdString(name));
    };
    m_assembler.onToolCallName =
        [this](int, const std::string &id, const std::string &name)
    {
        emit toolCallName(QString::fromStdString(id),
                          QString::fromStdString(name));
    };
    m_assembler.onToolCallArgsDelta = [this](int, const std::string &) {};
    m_assembler.onToolCallReady = [this](int, const std::string &id,
                                         const std::string &name,
                                         const std::string &argsJson)
    { m_recognizer.onToolCallReady(0, id, name, argsJson); };
    m_assembler.onFinish = [this](const std::string &) {};

    m_recognizer.onDispatch = [this](const std::string &id,
                                     const std::string &name,
                                     const nlohmann::json &args)
    {
        const QString s = QString::fromStdString(args.dump());
        const QJsonObject obj = QJsonDocument::fromJson(s.toUtf8()).object();
        emit toolCallReady(QString::fromStdString(id),
                           QString::fromStdString(name), obj);
    };
    m_recognizer.onInvalid = [this](const std::string &id,
                                    const std::string &name,
                                    const std::string &reason)
    {
        emit error(QStringLiteral("tool call %1 (%2) rejected: %3")
                       .arg(QString::fromStdString(id))
                       .arg(QString::fromStdString(name))
                       .arg(QString::fromStdString(reason)));
    };
}

void OpenAIClient::send(const QJsonArray &messages, const QJsonArray &tools)
{
    if (m_active.load(std::memory_order_relaxed))
    {
        emit error(QStringLiteral("a request is already in flight"));
        return;
    }
    if (m_apiKey.empty())
    {
        emit error(
            QStringLiteral("api key not set — configure it in Settings"));
        return;
    }

    json body;
    body["model"] = m_model;
    body["stream"] = m_streaming;
    body["messages"] = json::parse(
        QJsonDocument(messages).toJson(QJsonDocument::Compact).constData());
    if (!tools.isEmpty())
        body["tools"] = json::parse(
            QJsonDocument(tools).toJson(QJsonDocument::Compact).constData());

    m_sse.reset();
    m_assembler.reset();
    m_rawResponse.clear();
    m_httpCode = 0;
    m_cancel = false;

    if (m_worker.joinable())
        m_worker.join();

    m_active = true;
    const std::string bodyText = body.dump();
    const std::string url = m_baseUrl + "/chat/completions";
    logVerboseRequest(url, bodyText, messages.size(), tools.size());
    m_worker = std::thread([this, bodyText]() mutable { runRequest(bodyText); });
}

void OpenAIClient::cancel() { m_cancel = true; }

void OpenAIClient::logVerboseRequest(const std::string &url,
                                     const std::string &body,
                                     int messageCount, int toolCount) const
{
    if (!DebugTrace::verboseEnabled())
        return;
    logVerboseMultiline(
        "OpenAIClient",
        QStringLiteral("request url=%1 model=%2 stream=%3 messages=%4 tools=%5")
            .arg(QString::fromStdString(url))
            .arg(QString::fromStdString(m_model))
            .arg(m_streaming ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(messageCount)
            .arg(toolCount),
        QString::fromStdString(body));
}

void OpenAIClient::logVerboseResponse(const char *phase,
                                      const std::string &body) const
{
    if (!DebugTrace::verboseEnabled())
        return;
    logVerboseMultiline("OpenAIClient",
                        QStringLiteral("%1 body").arg(QString::fromUtf8(phase)),
                        QString::fromStdString(body));
}

void OpenAIClient::logVerboseChunk(const std::string &chunk) const
{
    if (!DebugTrace::verboseEnabled())
        return;
    logVerboseMultiline("OpenAIClient", QStringLiteral("stream chunk"),
                        QString::fromStdString(chunk));
}

#ifdef Q_OS_ANDROID
// Android: QNetworkAccessManager (system TLS, no curl dependency).
void OpenAIClient::runRequest(const std::string &body)
{
    const std::string url = m_baseUrl + "/chat/completions";

    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(QString::fromStdString(url)));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setRawHeader("Authorization",
                     QString::fromStdString("Bearer " + m_apiKey).toUtf8());
    if (m_streaming)
        req.setRawHeader("Accept", "text/event-stream");

    QNetworkReply *reply = mgr.post(req, QByteArray::fromStdString(body));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // For streaming, read chunks as they arrive.
    if (m_streaming)
    {
        QObject::connect(
            reply, &QNetworkReply::readyRead,
            [this, reply]()
            {
                if (m_cancel.load(std::memory_order_relaxed))
                {
                    reply->abort();
                    return;
                }
                QByteArray chunk = reply->readAll();
                if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toLongLong() >= 400)
                {
                    m_rawResponse.append(chunk.constData(), chunk.size());
                    return;
                }
                const std::string chunkText(chunk.constData(), chunk.size());
                logVerboseChunk(chunkText);
                m_sse.feed(chunkText);
            });
    }

    // Timeout: 10s connect (Qt handles this), 120s total for streaming.
    timer.start(m_streaming ? 120000 : 30000);
    loop.exec();

    if (!timer.isActive())
    {
        reply->abort();
        if (!m_cancel.load(std::memory_order_relaxed))
            emit error(QStringLiteral("HTTP request timed out"));
        else
            emit finished();
        m_active = false;
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        logVerboseResponse("error", m_rawResponse);
        if (m_cancel.load(std::memory_order_relaxed))
            emit finished();
        else
            emit error(QStringLiteral("HTTP: %1").arg(reply->errorString()));
        m_active = false;
        return;
    }

    m_httpCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toLongLong();

    if (!m_streaming)
    {
        const QByteArray bodyBytes = reply->readAll();
        m_rawResponse.assign(bodyBytes.constData(), bodyBytes.size());
        logVerboseResponse("response", m_rawResponse);
    }
    else
    {
        m_assembler.onStreamEnd();
    }

    if (m_httpCode >= 400)
    {
        logVerboseResponse("error", m_rawResponse);
        emit error(QStringLiteral("HTTP %1: %2")
                       .arg(m_httpCode)
                       .arg(QString::fromStdString(m_rawResponse).left(400)));
        m_active = false;
        return;
    }

    if (!m_streaming)
    {
        try
        {
            const json resp = json::parse(m_rawResponse);
            const auto &choices = resp.at("choices");
            if (choices.is_array() && !choices.empty())
            {
                const auto &msg = choices[0].value("message", json::object());
                if (msg.contains("content") && msg["content"].is_string())
                    emit contentDelta(QString::fromStdString(
                        msg["content"].get<std::string>()));
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array())
                {
                    for (const auto &tc : msg["tool_calls"])
                    {
                        const std::string id = tc.value("id", "");
                        const auto &fn = tc.value("function", json::object());
                        const std::string name = fn.value("name", "");
                        std::string args;
                        if (fn.contains("arguments"))
                        {
                            if (fn["arguments"].is_string())
                                args = fn["arguments"]
                                           .get_ref<const std::string &>();
                            else if (fn["arguments"].is_object())
                                args = fn["arguments"].dump();
                        }
                        m_recognizer.onToolCallReady(0, id, name, args);
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            logVerboseResponse("parse-failure", m_rawResponse);
            emit error(
                QStringLiteral("failed to parse response: %1").arg(e.what()));
            m_active = false;
            return;
        }
    }

    emit finished();
    m_active = false;
}
#else
// Desktop: libcurl
size_t OpenAIClient::writeCb(char *ptr, size_t sz, size_t nm, void *ud)
{
    auto *self = static_cast<OpenAIClient *>(ud);
    const size_t n = sz * nm;
    if (self->m_httpCode >= 400)
    {
        self->m_rawResponse.append(ptr, n);
        return n;
    }
    if (self->m_streaming)
    {
        const std::string chunk(ptr, n);
        self->logVerboseChunk(chunk);
        self->m_sse.feed(chunk);
    }
    else
    {
        self->m_rawResponse.append(ptr, n);
    }
    return n;
}

size_t OpenAIClient::headerCb(char *ptr, size_t sz, size_t nm, void *ud)
{
    auto *self = static_cast<OpenAIClient *>(ud);
    const size_t n = sz * nm;
    const std::string line(ptr, n);
    if (line.compare(0, 5, "HTTP/") == 0)
    {
        size_t sp = line.find(' ');
        if (sp != std::string::npos)
        {
            const size_t start = sp + 1;
            const size_t end = line.find(' ', start);
            const size_t len =
                (end == std::string::npos ? line.size() : end) - start;
            try
            {
                self->m_httpCode = std::stol(line.substr(start, len));
            }
            catch (...)
            {
            }
        }
    }
    return n;
}

int OpenAIClient::progressCb(void *ud, curl_off_t, curl_off_t, curl_off_t,
                             curl_off_t)
{
    auto *self = static_cast<OpenAIClient *>(ud);
    return self->m_cancel.load(std::memory_order_relaxed) ? 1 : 0;
}

void OpenAIClient::runRequest(const std::string &body)
{
    const std::string url = m_baseUrl + "/chat/completions";

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        emit error(QStringLiteral("curl_easy_init failed"));
        m_active = false;
        return;
    }

    struct curl_slist *hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    {
        std::string auth = "Authorization: Bearer " + m_apiKey;
        hdrs = curl_slist_append(hdrs, auth.c_str());
    }
    if (m_streaming)
        hdrs = curl_slist_append(hdrs, "Accept: text/event-stream");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &OpenAIClient::writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &OpenAIClient::headerCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &OpenAIClient::progressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode rc = curl_easy_perform(curl);
    curl_slist_free_all(hdrs);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
    {
        logVerboseResponse("error", m_rawResponse);
        if (m_cancel.load(std::memory_order_relaxed))
            emit finished();
        else
            emit error(QStringLiteral("curl: %1").arg(curl_easy_strerror(rc)));
        m_active = false;
        return;
    }

    if (httpCode >= 400)
    {
        logVerboseResponse("error", m_rawResponse);
        emit error(QStringLiteral("HTTP %1: %2")
                       .arg(httpCode)
                       .arg(QString::fromStdString(m_rawResponse).left(400)));
        m_active = false;
        return;
    }

    if (m_streaming)
    {
        m_assembler.onStreamEnd();
    }
    else
    {
        logVerboseResponse("response", m_rawResponse);
        try
        {
            const json resp = json::parse(m_rawResponse);
            const auto &choices = resp.at("choices");
            if (choices.is_array() && !choices.empty())
            {
                const auto &msg = choices[0].value("message", json::object());
                if (msg.contains("content") && msg["content"].is_string())
                    emit contentDelta(QString::fromStdString(
                        msg["content"].get<std::string>()));
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array())
                {
                    for (const auto &tc : msg["tool_calls"])
                    {
                        const std::string id = tc.value("id", "");
                        const auto &fn = tc.value("function", json::object());
                        const std::string name = fn.value("name", "");
                        std::string args;
                        if (fn.contains("arguments"))
                        {
                            if (fn["arguments"].is_string())
                                args = fn["arguments"]
                                           .get_ref<const std::string &>();
                            else if (fn["arguments"].is_object())
                                args = fn["arguments"].dump();
                        }
                        m_recognizer.onToolCallReady(0, id, name, args);
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            logVerboseResponse("parse-failure", m_rawResponse);
            emit error(
                QStringLiteral("failed to parse response: %1").arg(e.what()));
            m_active = false;
            return;
        }
    }

    emit finished();
    m_active = false;
}
#endif
