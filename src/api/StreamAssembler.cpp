#include "StreamAssembler.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

void StreamAssembler::onData(const std::string &jsonStr)
{
    if (m_finished)
        return; // ignore anything after finish_reason (some servers trail junk)

    json chunk;
    try
    {
        chunk = json::parse(jsonStr);
    }
    catch (...)
    {
        // Malformed chunk — skip. Real servers occasionally send partial JSON
        // split across write boundaries; the SseParser already reassembles by
        // event, so a parse failure here means a genuinely bad chunk.
        return;
    }

    const auto choicesIt = chunk.find("choices");
    if (choicesIt == chunk.end() || !choicesIt->is_array() ||
        choicesIt->empty())
        return;
    const auto &choices = *choicesIt;
    const auto &delta = choices[0].value("delta", json::object());

    // 1) text content
    if (delta.contains("content") && delta["content"].is_string())
    {
        const auto &s = delta["content"].get_ref<const std::string &>();
        if (!s.empty())
        {
            if (onContentDelta)
                onContentDelta(s);
        }
    }

    // 2) tool-call fragments — keyed by `index`
    if (delta.contains("tool_calls") && delta["tool_calls"].is_array())
    {
        for (const auto &tc : delta["tool_calls"])
        {
            const int index = tc.value("index", 0);
            auto &buf = m_toolCalls[index];

            // Capture name from ANY chunk that carries it — some endpoints
            // (Qwen/dashscope, DeepSeek) send function.name on a different
            // chunk than id. Capturing outside the id guard ensures buf.name
            // is populated regardless of chunk ordering.
            {
                const auto fnIt = tc.find("function");
                if (fnIt != tc.end() && fnIt->contains("name") &&
                    (*fnIt)["name"].is_string())
                {
                    const auto &nm =
                        (*fnIt)["name"].get_ref<const std::string &>();
                    // If the card was already created with an empty name and
                    // the name just arrived, patch it in place.
                    if (buf.started && buf.name.empty() && !nm.empty() &&
                        onToolCallName)
                        onToolCallName(index, buf.id, nm);
                    if (!nm.empty())
                        buf.name = nm;
                }
            }

            // First chunk for this index carries id + function.name. Some
            // OpenAI-compatible endpoints (DeepSeek, SiliconFlow, …) re-send
            // `id` on EVERY arguments fragment — guard with `started` so
            // onToolCallStart fires exactly once per tool call, not once per
            // chunk, otherwise the UI renders N duplicate "composing" cards.
            if (tc.contains("id") && tc["id"].is_string())
            {
                // Only accept a non-empty id. Some endpoints emit `"id": ""`
                // on trailing fragments, which would clobber the real id
                // captured on the first chunk and split the tool call across
                // two rows (composing card with the real id, ready/done card
                // with empty id) — the result then renders "below" the first
                // card instead of on it.
                const auto &newId = tc["id"].get_ref<const std::string &>();
                if (!newId.empty())
                    buf.id = newId;
                const bool firstSighting = !buf.started;
                buf.started = true;
                if (firstSighting && onToolCallStart)
                    onToolCallStart(index, buf.id, buf.name);
            }
            // Subsequent chunks carry function.arguments fragments (a JSON
            // string).
            const auto fnIt2 = tc.find("function");
            if (fnIt2 != tc.end() && fnIt2->contains("arguments") &&
                (*fnIt2)["arguments"].is_string())
            {
                const auto &frag =
                    (*fnIt2)["arguments"].get_ref<const std::string &>();
                if (!frag.empty())
                {
                    buf.args.append(frag);
                    if (onToolCallArgsDelta)
                        onToolCallArgsDelta(index, frag);
                }
            }
        }
    }

    // 3) finish_reason
    const auto frIt = choices[0].find("finish_reason");
    if (frIt != choices[0].end() && !frIt->is_null())
    {
        const std::string reason = frIt->get<std::string>();
        m_finished = true;
        if (onFinish)
            onFinish(reason);
        if (reason == "tool_calls")
            flushToolCalls();
    }
}

void StreamAssembler::onStreamEnd()
{
    if (!m_finished)
    {
        if (onFinish)
            onFinish(""); // transport ended without an explicit reason
        m_finished = true;
    }
    // Defensive flush: if the model emitted tool_calls but the server dropped
    // finish_reason (shouldn't happen with OpenAI), still surface what we have.
    flushToolCalls();
}

void StreamAssembler::reset()
{
    m_toolCalls.clear();
    m_finished = false;
    m_toolCallsFlushed = false;
}

void StreamAssembler::flushToolCalls()
{
    if (m_toolCallsFlushed)
        return;
    for (const auto &kv : m_toolCalls)
        flushToolCall(kv.first, kv.second);
    m_toolCallsFlushed = true;
}

void StreamAssembler::flushToolCall(int index, const ToolCallBuf &buf)
{
    if (!buf.started)
        return; // never saw an id — nothing coherent to dispatch
    // If the endpoint never sent an id (or sent "" and we refused to clobber),
    // synthesize a unique one so multiple id-less calls don't collide onto a
    // single row via indexOfTool("").
    std::string id = buf.id;
    if (id.empty())
        id = "call_synth_" + std::to_string(m_nextSynthId++);
    if (onToolCallReady)
        onToolCallReady(index, id, buf.name, buf.args);
}
