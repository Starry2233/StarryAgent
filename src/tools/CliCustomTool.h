#pragma once

#include "Tool.h"

#include <QProcessEnvironment>
#include <QStringList>

class CliCustomTool : public Tool
{
  public:
    CliCustomTool(QString publicId, QString description, nlohmann::json schema,
                  QString command, QStringList args, QString cwd,
                  QProcessEnvironment env, QString inputMode,
                  bool approvalRequired);

    QString id() const override { return m_id; }
    QString description() const override { return m_description; }
    nlohmann::json schema() const override { return m_schema; }
    bool permissionRequired() const override { return m_permissionRequired; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_id;
    QString m_description;
    nlohmann::json m_schema;
    QString m_command;
    QStringList m_args;
    QString m_cwd;
    QProcessEnvironment m_env;
    QString m_inputMode;
    bool m_permissionRequired = true;
};
