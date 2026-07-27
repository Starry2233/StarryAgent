#pragma once

#include "Tool.h"

#include <QString>

// Send an Android broadcast intent. Android-only — returns "Unavailable" on
// desktop platforms (Windows/macOS/Linux).
class BroadcastTool : public Tool
{
  public:
    BroadcastTool() = default;

    QString id() const override { return QStringLiteral("broadcast"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;
};
