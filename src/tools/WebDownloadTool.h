#pragma once

#include "Tool.h"

#include <QString>

#include <utility>

class WebDownloadTool : public Tool
{
  public:
    explicit WebDownloadTool(QString workspace)
        : m_workspace(std::move(workspace))
    {
    }

    QString id() const override { return QStringLiteral("web_download"); }
    QString description() const override;
    nlohmann::json schema() const override;
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
