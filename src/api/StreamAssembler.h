#pragma once

#include <functional>
#include <map>
#include <string>

// StreamAssembler — turns SSE JSON chunks into a coherent assistant turn.
//
// Per OpenAI streaming spec, each `data:` payload is a chat.completion.chunk:
//   {"choices":[{"delta":{...},"finish_reason":null|"..."}]}
// delta.content is a text fragment (append to live assistant text).
// delta.tool_calls[] is a list keyed by `index`:
//   - the FIRST chunk for an index carries `id` + `function.name`
//   - subsequent chunks carry fragments of `function.arguments` (a JSON string)
//
// The assembler buffers arguments fragments per index. It emits onToolCallReady
// ONLY when the stream signals completion (finish_reason == "tool_calls") or
// when the stream ends. Partial arguments never leave this object — that is
// the foundation of the dispatch rule enforced by ToolCallRecognizer.
class StreamAssembler
{
  public:
    // Feed one `data:` JSON payload (the string SseParser handed to onData).
    void onData(const std::string &jsonStr);
    // Call when the transport says the stream is over (e.g. [DONE] or curl EOF)
    // even if no finish_reason was seen. Flushes any pending tool calls.
    void onStreamEnd();
    void reset();

    std::function<void(const std::string &text)> onContentDelta;
    std::function<void(int index, const std::string &id,
                       const std::string &name)>
        onToolCallStart;
    // Fires when the name is captured AFTER onToolCallStart already fired with
    // an empty name (endpoints that split id and function.name across chunks).
    // Lets the UI patch the card's toolName in place instead of showing "".
    std::function<void(int index, const std::string &id,
                       const std::string &name)>
        onToolCallName;
    std::function<void(int index, const std::string &fragment)>
        onToolCallArgsDelta;
    // Emitted with the FULL accumulated arguments JSON string, once, per tool
    // call — never on a partial buffer.
    std::function<void(int index, const std::string &id,
                       const std::string &name, const std::string &argsJson)>
        onToolCallReady;
    std::function<void(const std::string &reason)> onFinish;

  private:
    struct ToolCallBuf
    {
        std::string id;
        std::string name;
        std::string args;     // accumulated function.arguments fragments
        bool started = false; // have we seen the first chunk (with id/name) yet
    };

    void flushToolCalls();
    void flushToolCall(int index, const ToolCallBuf &buf);

    std::map<int, ToolCallBuf> m_toolCalls;
    bool m_finished = false;
    bool m_toolCallsFlushed = false;
    int m_nextSynthId = 0; // for endpoints that omit tool_call ids entirely
};
