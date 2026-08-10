#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "backend/BackendSession.h"
#include "chat/ConversationManager.h"
#include "core/Config.h"
#include "core/Settings.h"
#include "tools/ToolRegistry.h"

using namespace StarryAgent;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Config config;
    Settings settings(&config);
    if (!config.firstLaunch())
        settings.load();

    ToolRegistry toolRegistry(&config, &settings);
    ConversationManager conversations(&config, &settings, &toolRegistry, true);
    BackendSession session(&conversations, &settings);

    QTextStream output(stdout);
    QObject::connect(&session, &BackendSession::eventReady, &app,
                     [&output](const QJsonObject &event)
                     {
                         output << QJsonDocument(event).toJson(QJsonDocument::Compact)
                                << Qt::endl;
                         output.flush();
                     });

    QTextStream input(stdin);
    while (!input.atEnd())
    {
        const QString line = input.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8());
        if (!document.isObject())
            continue;
        const QJsonObject response = session.handleRequest(document.object());
        output << QJsonDocument(response).toJson(QJsonDocument::Compact)
               << Qt::endl;
        output.flush();
        QCoreApplication::processEvents();
    }
    return 0;
}
