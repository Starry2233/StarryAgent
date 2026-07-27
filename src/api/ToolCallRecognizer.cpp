#include "ToolCallRecognizer.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Some OpenAI-compatible endpoints (DeepSeek reasoning models in particular)
// wrap the function arguments in markdown code fences or surround them with
// prose, even though the JSON body itself is valid. Extract the first balanced
// JSON object so the recognizer doesn't reject a well-formed call.
static std::string extractJsonObject(const std::string &s)
{
    if (s.empty())
        return "{}";
    // Fast path: already a clean object.
    {
        try
        {
            json test = json::parse(s);
            if (test.is_object())
                return s;
        }
        catch (...)
        {
            // fall through to extraction
        }
    }
    // Strip markdown code fences: ```json\n ... \n```  or  ```\n ... \n```
    std::string t = s;
    // Remove leading ```lang and trailing ```
    {
        auto pos = t.find("```");
        if (pos != std::string::npos)
        {
            // Erase the opening fence + optional language tag up to newline.
            auto nl = t.find('\n', pos);
            if (nl != std::string::npos)
                t.erase(0, nl + 1);
            else
                t.erase(0, pos + 3);
            // Erase the closing fence.
            auto close = t.rfind("```");
            if (close != std::string::npos)
                t.erase(close);
        }
    }
    // Find the first '{' and try to parse forward to its matching '}'.
    auto start = t.find('{');
    if (start == std::string::npos)
        return s;
    // Walk from 'start' tracking brace depth (ignoring braces inside strings).
    int depth = 0;
    bool inStr = false;
    bool esc = false;
    for (size_t i = start; i < t.size(); ++i)
    {
        char c = t[i];
        if (inStr)
        {
            if (esc)
            {
                esc = false;
                continue;
            }
            if (c == '\\')
            {
                esc = true;
                continue;
            }
            if (c == '"')
                inStr = false;
            continue;
        }
        if (c == '"')
        {
            inStr = true;
            continue;
        }
        if (c == '{')
            ++depth;
        else if (c == '}')
        {
            --depth;
            if (depth == 0)
            {
                std::string sub = t.substr(start, i - start + 1);
                try
                {
                    json test = json::parse(sub);
                    if (test.is_object())
                        return sub;
                }
                catch (...)
                {
                    // keep scanning
                }
            }
        }
    }
    // Non-empty arguments that do not contain a complete object are malformed;
    // return the original string so the parse below rejects instead of silently
    // turning it into an empty argument object.
    return s;
}

void ToolCallRecognizer::onToolCallReady(int /*index*/, const std::string &id,
                                         const std::string &name,
                                         const std::string &argsJson)
{
    // Empty arguments is legal (a tool with no parameters). Treat "" as "{}".
    const std::string &src = argsJson.empty() ? std::string("{}") : argsJson;

    // Some endpoints wrap the JSON in markdown fences or prose; extract the
    // real object before parsing so a well-formed call isn't rejected.
    const std::string cleaned = extractJsonObject(src);

    nlohmann::json args;
    try
    {
        args = nlohmann::json::parse(cleaned);
    }
    catch (const std::exception &e)
    {
        if (onInvalid)
            onInvalid(id, name,
                      std::string("invalid arguments JSON: ") + e.what());
        return;
    }
    if (!args.is_object())
    {
        if (onInvalid)
            onInvalid(id, name, "arguments must be a JSON object");
        return;
    }
    if (onDispatch)
        onDispatch(id, name, args);
}
