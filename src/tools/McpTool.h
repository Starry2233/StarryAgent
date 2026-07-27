#pragma once

#include "Tool.h"

#include <QProcessEnvironment>
#include <QStringList>

class McpTool : public Tool
{
  public:
    struct ServerConfig
    {
        QString serverId;
        QString command;
        QStringList args;
        QString cwd;
        QProcessEnvironment env;
        int timeoutMs = 30000;
        bool approvalRequired = true;
    };

    McpTool(ServerConfig serverConfig, QString publicId, QString remoteName,
            QString description, nlohmann::json schema);

    QString id() const override { return m_publicId; }
    QString description() const override { return m_description; }
    nlohmann::json schema() const override { return m_schema; }
    bool permissionRequired() const override
    {
        return m_serverConfig.approvalRequired;
    }
    QString execute(const nlohmann::json &args) override;

    static std::vector<std::unique_ptr<McpTool>>
    discover(const ServerConfig &config, QString *errorMessage = nullptr);

  private:
    ServerConfig m_serverConfig;
    QString m_publicId;
    QString m_remoteName;
    QString m_description;
    nlohmann::json m_schema;
};
