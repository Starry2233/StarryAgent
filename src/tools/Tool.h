#pragma once

#include <QString>
#include <nlohmann/json.hpp>

// Abstract tool interface. Each built-in tool implements this; the registry
// owns instances and dispatches by id. Custom mcp/cli tools (Phase 6) are
// adapted to this same shape.
//
// A tool's `execute` returns the string that becomes the `content` of the
// role:"tool" message handed back to the model. Encode errors in the string
// (e.g. "Error: ...") so the model can read them — there is no separate
// out-of-band error channel for tool results in the OpenAI shape.
class Tool
{
  public:
    virtual ~Tool() = default;

    virtual QString id() const = 0;
    virtual QString description() const = 0;
    // JSON schema for the "parameters" field of the OpenAI tool envelope.
    virtual nlohmann::json schema() const = 0;
    // Whether the tool requires per-call user approval. Destructive tools
    // return true (the default); read-only tools may override to false.
    virtual bool permissionRequired() const { return true; }
    // Run the tool. Must be safe to call on a worker thread. The returned
    // string is the tool result content (success or error message).
    virtual QString execute(const nlohmann::json &args) = 0;
};
