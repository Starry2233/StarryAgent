#pragma once

#include "Tool.h"

#include <QString>

class WebFetchTool : public Tool
{
  public:
    WebFetchTool() = default;

    QString id() const override { return QStringLiteral("web_fetch"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return false; }
    QString execute(const nlohmann::json &args) override;
};
