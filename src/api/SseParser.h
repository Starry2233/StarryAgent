#pragma once

#include <functional>
#include <string>

// SseParser — RFC-draft SSE byte-stream → `data:` payloads.
//
// Feed raw bytes from any source (libcurl WRITEFUNCTION, a test fixture, …).
// The parser buffers unterminated tail and splits events on blank lines.
// OpenAI sends one `data:` line per event terminated by `\n\n`; the parser
// also handles `\r\n` and the spec's multi-line concatenation (rare here).
//
// `data: [DONE]` (OpenAI's stream terminator) fires onDone() instead of onData.
class SseParser
{
  public:
    // Append raw bytes. May invoke onData/onDone zero or more times.
    void feed(const std::string &bytes);
    // Discard any in-flight state. Call between requests.
    void reset();

    std::function<void(const std::string &data)>
        onData;                   // one complete `data:` payload
    std::function<void()> onDone; // `data: [DONE]` received

  private:
    void processLine(const std::string &line);
    void dispatchEvent();

    std::string m_buf; // bytes not yet terminated by \n
    std::string
        m_eventData; // concatenated `data:` payloads for the current event
    bool m_haveData =
        false; // does m_eventData hold anything (vs. comments/empty)
};
