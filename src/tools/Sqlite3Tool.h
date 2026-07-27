#pragma once

#include "Tool.h"

#include <QString>

class Sqlite3Tool : public Tool
{
  public:
    Sqlite3Tool(const QString &workspace);

    QString id() const override { return QStringLiteral("sqlite3"); }
    QString description() const override;
    nlohmann::json schema() const override;
    bool permissionRequired() const override { return true; }
    QString execute(const nlohmann::json &args) override;

  private:
    QString m_workspace;
};
