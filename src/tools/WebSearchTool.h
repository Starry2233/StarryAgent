#pragma once

#include "Tool.h"

class Settings;

class WebSearchTool : public Tool
{
  public:
    explicit WebSearchTool(Settings *settings = nullptr)
        : m_settings(settings)
    {
    }
    QString id() const override { return QStringLiteral("web_search"); }
    QString description() const override;
    nlohmann::json schema() const override;
    QString execute(const nlohmann::json &args) override;

  private:
    Settings *m_settings = nullptr;
};
