#include "BroadcastTool.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

QString BroadcastTool::description() const
{
    return QStringLiteral(
        "Send an Android broadcast intent. Android-only. On desktop platforms "
        "(Windows/macOS/Linux) returns 'Unavailable'.");
}

json BroadcastTool::schema() const
{
    return {
        {"type", "object"},
        {"properties",
         {
             {"action",
              {{"type", "string"},
               {"description", "The intent action string (e.g. "
                               "android.intent.action.BATTERY_LOW)."}}},
             {"extras",
              {{"type", "object"},
               {"description", "Optional key-value pairs of intent extras."}}},
         }},
        {"required", {"action"}},
    };
}

QString BroadcastTool::execute(const json &)
{
    // Android implementation lands in Phase 9 (JNI/Shizuku). Desktop returns
    // unavailable so the model learns the platform capability.
    return QStringLiteral("Unavailable: broadcast is an Android-only tool.");
}
