#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <string>

// ToolCallRecognizer — the gate that turns a "tool call is complete" event
// into a "dispatch this tool call" event.
//
// The rule (CLAUDE.md, critical): never dispatch on partial arguments. The
// assembler delivers onToolCallReady ONLY after the full arguments JSON string
// has been accumulated, so this class's job is narrower: parse the arguments
// string, and if it's valid JSON, hand it to onDispatch; if not, reject via
// onInvalid. Schema validation against the tool definition is layered in later
// (Phase 4, once the ToolRegistry exists); until then we validate shape only.
class ToolCallRecognizer
{
  public:
    // index is the OpenAI streaming index (informational), id is the tool_call
    // id (used later as tool_call_id in the result message), name is the
    // function name to look up in the registry, argsJson is the FULLY
    // accumulated arguments string.
    void onToolCallReady(int index, const std::string &id,
                         const std::string &name, const std::string &argsJson);

    std::function<void(const std::string &id, const std::string &name,
                       const nlohmann::json &args)>
        onDispatch;
    std::function<void(const std::string &id, const std::string &name,
                       const std::string &reason)>
        onInvalid;
};
