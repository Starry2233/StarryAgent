#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#ifdef Q_OS_ANDROID
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#else
#include <curl/curl.h>
#endif

#include "SseParser.h"
#include "StreamAssembler.h"
#include "ToolCallRecognizer.h"

// OpenAIClient — libcurl-backed client for OpenAI Chat Completions.
//
// One adapter covers OpenAI, DeepSeek, and any OpenAI-compatible endpoint
// (DeepSeek exposes an OpenAI-compatible API). baseUrl/apiKey/model are
// user-configurable via Settings.
//
// Threading: send() spawns a worker thread that runs curl_easy_perform. The
// curl WRITEFUNCTION callback fires on the worker thread and feeds SseParser,
// which feeds StreamAssembler, which feeds ToolCallRecognizer — all on the
// worker thread. The Qt signals below are emitted from the worker thread;
// because OpenAIClient lives on the UI thread, Qt auto-queues each emit to
// the UI thread's event loop (safe, no manual marshalling).
//
// Tool-call dispatch rule: toolCallReady fires ONLY after the full arguments
// JSON has been accumulated and validated. toolCallComposing fires during
// streaming to surface "args still arriving" in the UI; it MUST NOT trigger
// execution.
class OpenAIClient : public QObject
{
    Q_OBJECT
  public:
    explicit OpenAIClient(QObject *parent = nullptr);
    ~OpenAIClient() override;

    // Pull from Settings before send().
    void setBaseUrl(const QString &url) { m_baseUrl = url.toStdString(); }
    void setApiKey(const QString &key) { m_apiKey = key.toStdString(); }
    void setModel(const QString &model) { m_model = model.toStdString(); }
    void setStreaming(bool on) { m_streaming = on; }

    // Non-blocking. messages = OpenAI message array; tools = OpenAI tool
    // schema array (may be empty). If a request is already in flight, this
    // emits error("busy") and returns without starting a new one.
    Q_INVOKABLE void send(const QJsonArray &messages,
                          const QJsonArray &tools = QJsonArray());

    // Best-effort abort of the in-flight request (curl XFERINFOFUNCTION).
    Q_INVOKABLE void cancel();

  signals:
    // A text fragment for the live assistant message. Append to your chat view.
    void contentDelta(const QString &text);
    void reasoningDelta(const QString &text);
    // A tool call has STARTED streaming its arguments. Do NOT execute. Render
    // as a "composing" card.
    void toolCallComposing(const QString &id, const QString &name);
    // The tool name arrived after the composing card was already created with
    // an empty name (endpoints that split id and function.name across chunks).
    // Patch the card's toolName in place.
    void toolCallName(const QString &id, const QString &name);
    // A tool call's arguments are complete and JSON-valid. Safe to execute.
    void toolCallReady(const QString &id, const QString &name,
                       const QJsonObject &args);
    // Stream finished (text + any tool calls fully delivered). No more signals
    // after this until the next send().
    void finished();
    // Transport/HTTP error. The request is over; finished() is NOT emitted.
    void error(const QString &message);

  private:
    void wirePipeline();
    void runRequest(const std::string &body);
    void handleStreamingPayload(const std::string &data);
    void emitReasoningFromMessage(const nlohmann::json &message);
    void parseToolCallsFromMessage(const nlohmann::json &message);
    void logVerboseRequest(const std::string &url, const std::string &body,
                           int messageCount, int toolCount) const;
    void logVerboseResponse(const char *phase, const std::string &body) const;
    void logVerboseChunk(const std::string &chunk) const;

#ifndef Q_OS_ANDROID
    static size_t writeCb(char *ptr, size_t sz, size_t nm, void *ud);
    static size_t headerCb(char *ptr, size_t sz, size_t nm, void *ud);
    static int progressCb(void *ud, curl_off_t dltotal, curl_off_t dlnow,
                          curl_off_t ultotal, curl_off_t ulnow);
#endif

    std::string m_baseUrl = "https://api.openai.com/v1";
    std::string m_apiKey;
    std::string m_model = "gpt-4o-mini";
    bool m_streaming = true;

    SseParser m_sse;
    StreamAssembler m_assembler;
    ToolCallRecognizer m_recognizer;

    std::thread m_worker;
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_cancel{false};
    std::string m_rawResponse; // non-streaming path + error bodies (streaming)
    long m_httpCode{0};        // captured so writeCb can branch on 4xx
};
