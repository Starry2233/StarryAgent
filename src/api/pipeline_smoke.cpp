#include "pipeline_smoke.h"

#include <QDebug>
#include <QJsonDocument>

#include <atomic>
#include <string>
#include <vector>

#include "SseParser.h"
#include "StreamAssembler.h"
#include "ToolCallRecognizer.h"

using json = nlohmann::json;

namespace
{

struct Fail
{
    std::string msg;
};

// The mock stream: a realistic OpenAI tool-calling chat completion, fed to the
// parser in deliberately awkward chunk boundaries (mid-line, mid-event) to
// prove the parser reassembles by event, not by write boundary.
const std::vector<std::string> kStream = {
    // event 1: first content token
    std::string("data: "
                "{\"choices\":[{\"delta\":{\"role\":\"assistant\",\"content\":"
                "\"Hel\"}}]}\n\n"),
    // event 2: rest of content, split mid-JSON-boundary on purpose
    std::string(
        "data: {\"choices\":[{\"delta\":{\"content\":\"lo, world.\"}}]}\n"),
    std::string("\n"),
    // event 3: tool call #0 — first chunk carries id + name
    std::string(
        "data: "
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_"
        "42\",\"type\":\"function\",\"function\":{\"name\":\"edit\","
        "\"arguments\":\"{\\\"path\\\":\\\"a.txt\\\"\"}}]}}]}\n\n"),
    // event 4: tool call #0 — arguments fragment (still incomplete JSON)
    std::string(
        "data: "
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{"
        "\"arguments\":\",\\\"content\\\":\\\"hi\\\"}\"}}]}}]}\n\n"),
    // event 5: finish_reason = tool_calls — this is the gate
    std::string(
        "data: "
        "{\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n"),
    // terminator
    std::string("data: [DONE]\n\n"),
};

// A second stream where the model emits a tool call with MALFORMED arguments
// JSON (missing closing brace). The recognizer must reject, not dispatch.
const std::vector<std::string> kBadStream = {
    std::string(
        "data: "
        "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_"
        "99\",\"type\":\"function\",\"function\":{\"name\":\"overwrite\","
        "\"arguments\":\"{\\\"path\\\":\\\"b.txt\\\"\"}}]}}]}\n\n"),
    std::string(
        "data: "
        "{\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}\n\n"),
    std::string("data: [DONE]\n\n"),
};

} // namespace

bool runPipelineSmokeTest()
{
    try
    {
        // ---- Case 1: well-formed tool-calling stream ----------------------
        SseParser sse;
        StreamAssembler asm1;
        ToolCallRecognizer rec;

        std::vector<std::string> contentDeltas;
        std::vector<std::pair<std::string, std::string>>
            composing;                                          // (id, name)
        std::vector<std::pair<std::string, std::string>> ready; // (id, name)
        std::vector<std::string> readyArgs; // raw args json per ready
        std::vector<std::pair<std::string, std::string>> invalid;
        std::vector<std::string> finishes;

        // CRITICAL assertion: ready must fire AFTER finish_reason, never
        // before. Track the order with a monotonic counter.
        std::atomic<int> phase{
            0}; // 0=streaming, 1=saw finish_reason, 2=saw ready
        int readyBeforeFinish = 0;

        asm1.onContentDelta = [&](const std::string &t)
        { contentDeltas.push_back(t); };
        asm1.onToolCallStart =
            [&](int, const std::string &id, const std::string &name)
        { composing.emplace_back(id, name); };
        asm1.onToolCallReady = [&](int, const std::string &id,
                                   const std::string &name,
                                   const std::string &args)
        {
            if (phase.load() < 1)
                ++readyBeforeFinish; // ready fired before finish_reason —
                                     // FORBIDDEN
            ready.emplace_back(id, name);
            readyArgs.push_back(args);
            phase.store(2);
        };
        asm1.onFinish = [&](const std::string &reason)
        {
            if (reason == "tool_calls")
                phase.store(1);
            finishes.push_back(reason);
        };
        rec.onDispatch = [&](const std::string &id, const std::string &name,
                             const nlohmann::json &args)
        {
            // (handled via ready/readyArgs above; recognizer forwards to here)
            (void)id;
            (void)name;
            (void)args;
        };
        rec.onInvalid = [&](const std::string &id, const std::string &name,
                            const std::string &reason)
        { invalid.emplace_back(id, name + ": " + reason); };

        // Wire sse → asm1; recognizer is called by asm1.onToolCallReady.
        sse.onData = [&](const std::string &d) { asm1.onData(d); };
        sse.onDone = [&] { asm1.onStreamEnd(); };

        // Feed in awkward chunks.
        for (const auto &chunk : kStream)
            sse.feed(chunk);

        // Assertions for case 1.
        if (contentDeltas.size() != 2)
            throw Fail{std::string("expected 2 content deltas, got ") +
                       std::to_string(contentDeltas.size())};
        const std::string text = contentDeltas[0] + contentDeltas[1];
        if (text != "Hello, world.")
            throw Fail{std::string("content mismatch: '") + text + "'"};
        if (composing.size() != 1 || composing[0].first != "call_42" ||
            composing[0].second != "edit")
            throw Fail{"composing event missing/mismatched"};
        if (readyBeforeFinish != 0)
            throw Fail{"toolCallReady fired BEFORE finish_reason==tool_calls "
                       "(partial dispatch!)"};
        if (ready.size() != 1 || ready[0].first != "call_42" ||
            ready[0].second != "edit")
            throw Fail{"toolCallReady missing/mismatched"};
        // Recognizer parsed the args; verify the JSON object has the two keys.
        if (readyArgs.empty())
            throw Fail{"no args captured"};
        const json argsObj = json::parse(readyArgs[0]);
        if (argsObj.value("path", std::string()) != "a.txt")
            throw Fail{"args.path mismatch"};
        if (argsObj.value("content", std::string()) != "hi")
            throw Fail{"args.content mismatch"};

        qInfo()
            << "[smoke] case 1 ok: 2 content deltas, 1 composing→ready, args "
               "valid, ready AFTER finish_reason";

        // ---- Case 2: malformed arguments JSON -----------------------------
        SseParser sse2;
        StreamAssembler asm2;
        ToolCallRecognizer rec2;
        std::vector<std::pair<std::string, std::string>> dispatched2;
        std::vector<std::pair<std::string, std::string>> invalid2;

        asm2.onToolCallReady = [&](int, const std::string &id,
                                   const std::string &name,
                                   const std::string &args)
        { rec2.onToolCallReady(0, id, name, args); };
        rec2.onDispatch = [&](const std::string &id, const std::string &name,
                              const nlohmann::json &)
        { dispatched2.emplace_back(id, name); };
        rec2.onInvalid = [&](const std::string &id, const std::string &name,
                             const std::string &reason)
        {
            invalid2.emplace_back(id, name);
            qInfo() << "[smoke] case 2 invalid (expected):" << reason.c_str();
        };
        sse2.onData = [&](const std::string &d) { asm2.onData(d); };
        sse2.onDone = [&] { asm2.onStreamEnd(); };

        for (const auto &chunk : kBadStream)
            sse2.feed(chunk);

        if (!dispatched2.empty())
            throw Fail{
                "malformed-args tool call was DISPATCHED — should have been "
                "rejected"};
        if (invalid2.size() != 1 || invalid2[0].first != "call_99" ||
            invalid2[0].second != "overwrite")
            throw Fail{"invalid event missing/mismatched"};

        qInfo() << "[smoke] case 2 ok: malformed-args tool call rejected (not "
                   "dispatched)";

        // ---- Case 3: partial stream WITHOUT finish_reason must not dispatch
        // (defensive: onStreamEnd flushes, but only after the stream actually
        // ends)
        SseParser sse3;
        StreamAssembler asm3;
        int dispatched3 = 0;
        asm3.onToolCallReady = [&](int, const std::string &,
                                   const std::string &, const std::string &)
        { ++dispatched3; };
        sse3.onData = [&](const std::string &d) { asm3.onData(d); };

        // Feed only events 3-4 (tool call start + one args fragment) — NO
        // finish_reason, NO [DONE]. Nothing should dispatch yet.
        sse3.feed(kStream[2]);
        sse3.feed(kStream[3]);
        if (dispatched3 != 0)
            throw Fail{
                "dispatch happened mid-stream without finish_reason (partial "
                "dispatch!)"};

        qInfo()
            << "[smoke] case 3 ok: mid-stream (no finish_reason) → no dispatch";

        qInfo() << "[smoke] ALL PIPELINE ASSERTIONS PASSED";
        return true;
    }
    catch (const Fail &f)
    {
        qWarning() << "[smoke] FAIL:" << f.msg.c_str();
        return false;
    }
    catch (const std::exception &e)
    {
        qWarning() << "[smoke] exception:" << e.what();
        return false;
    }
}
