#include "tools_smoke.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTimer>

#include <nlohmann/json.hpp>

#include "ToolRegistry.h"
#include "core/Config.h"
#include "core/Settings.h"

bool runToolsSmokeTest()
{
    QCoreApplication *app = QCoreApplication::instance();
    Config config;
    if (config.firstLaunch())
        config.setRoot(config.defaultRoot());

    ToolRegistry registry(&config);

    // 1) print the tools array the model would receive
    const QJsonArray tools = registry.openaiToolsArray();
    qInfo() << "[tools] openaiToolsArray has" << tools.size() << "entries:";
    for (const QJsonValue &v : tools)
    {
        const QJsonObject fn = v.toObject().value("function").toObject();
        qInfo() << "   -" << fn.value("name").toString() << ":"
                << fn.value("description").toString().left(60) << "...";
    }
    if (tools.size() < 3)
    {
        qWarning() << "[tools] expected >= 3 tools, got" << tools.size();
        return false;
    }
    if (!registry.exists("edit") || !registry.exists("overwrite") ||
        !registry.exists("exec") || !registry.exists("web_search") ||
        !registry.exists("web_fetch") || !registry.exists("web_download") ||
        !registry.exists("recall_memory") || !registry.exists("write_memory"))
    {
        qWarning() << "[tools] required built-ins are not all registered";
        return false;
    }

    // 2-4) dispatch each tool SEQUENTIALLY. edit depends on overwrite's output,
    // so they can't race. (Parallel dispatch is supported by the registry —
    // each call spawns its own thread — but the test's data dependency needs
    // ordering.)
    struct Case
    {
        QString toolCallId;
        QString name;
        QJsonObject args;
        QString expectSubstring; // result must contain this (else "Error:")
    };
    const QList<Case> cases = {
        {"tc1", "overwrite",
         QJsonObject{{"path", "smoke_test.txt"}, {"content", "hello world"}},
         "wrote"},
        {"tc2", "edit",
         QJsonObject{{"path", "smoke_test.txt"},
                     {"oldText", "hello"},
                     {"newText", "goodbye"}},
         "edited"},
        {"tc3", "exec", QJsonObject{{"command", "echo starryagent-smoke"}},
         "starryagent-smoke"},
        {"tc4", "write_memory",
         QJsonObject{{"title", "smoke memory"},
                     {"content", "starryagent-memory-smoke"},
                     {"key", "smoke-memory"},
                     {"scope", "conversation"}},
         "Memory saved."},
        {"tc5", "recall_memory",
         QJsonObject{{"query", "staryagent memry smoke"},
                     {"limit", 3},
                     {"scope", "conversation"}},
         "starryagent-memory-smoke"},
        {"tc6", "web_search", QJsonObject{{"query", "OpenAI"}, {"limit", 3}},
         "Web search results for query:"},
    };

    QMap<QString, QString> results;
    bool ok = true;
    for (const auto &c : cases)
    {
        QEventLoop loop;
        QString captured;
        QObject::connect(&registry, &ToolRegistry::toolFinished, app,
                         [&](const QString &ownerId, const QString &tcid,
                             const QString &, const QString &result)
                         {
                             if (ownerId == QStringLiteral("smoke") &&
                                 tcid == c.toolCallId)
                             {
                                 captured = result;
                                 loop.quit();
                             }
                         });
        registry.dispatch(QStringLiteral("smoke"), c.toolCallId, c.name,
                          c.args);
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();

        qInfo() << "[tools] finished" << c.name << "→" << captured.left(120);
        results[c.toolCallId] = captured;
        if (captured.isEmpty())
        {
            qWarning() << "[tools]" << c.name << "did not finish (timeout)";
            ok = false;
        }
        else if (captured.startsWith("Error:", Qt::CaseInsensitive) ||
                 !captured.contains(c.expectSubstring, Qt::CaseInsensitive))
        {
            qWarning() << "[tools]" << c.name
                       << "result unexpected:" << captured;
            ok = false;
        }
    }

    // cleanup the test file
    QFile::remove(config.workspacePath() + QDir::separator() +
                  "smoke_test.txt");
    QFile::remove(config.memoriesPath() + QDir::separator() + "conversations" +
                  QDir::separator() + "smoke" + QDir::separator() +
                  "smoke-memory.md");

    if (ok)
        qInfo() << "[tools] ALL TOOL ASSERTIONS PASSED";
    else
        qWarning() << "[tools] SOME ASSERTIONS FAILED";
    return ok;
}
