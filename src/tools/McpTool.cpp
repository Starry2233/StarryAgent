#include "McpTool.h"
#include "ToolWorkdir.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
QByteArray encodeMessage(const json &message)
{
    const QByteArray body = QByteArray::fromStdString(message.dump());
    return QByteArray("Content-Length: ") + QByteArray::number(body.size()) +
           QByteArray("\r\n\r\n") + body;
}

bool readOneMessage(QProcess &process, QByteArray &buffer, json *message,
                    int timeoutMs, QString *errorMessage)
{
    QElapsedTimer timer;
    timer.start();

    auto parseBuffered = [&]() -> bool
    {
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return false;

        int contentLength = -1;
        const QList<QByteArray> headers = buffer.left(headerEnd).split('\n');
        for (QByteArray header : headers)
        {
            header = header.trimmed();
            const QByteArray prefix("Content-Length:");
            if (header.startsWith(prefix))
            {
                bool ok = false;
                contentLength = header.mid(prefix.size()).trimmed().toInt(&ok);
                if (!ok)
                    contentLength = -1;
            }
        }
        if (contentLength < 0)
        {
            if (errorMessage)
                *errorMessage = QStringLiteral(
                    "Error: MCP response missing Content-Length.");
            return false;
        }

        const int bodyStart = headerEnd + 4;
        if (buffer.size() - bodyStart < contentLength)
            return false;

        const QByteArray body = buffer.mid(bodyStart, contentLength);
        buffer.remove(0, bodyStart + contentLength);
        try
        {
            *message = json::parse(body.constData());
            return true;
        }
        catch (const std::exception &e)
        {
            if (errorMessage)
                *errorMessage =
                    QStringLiteral("Error: failed to parse MCP JSON: %1")
                        .arg(QString::fromUtf8(e.what()));
            return false;
        }
    };

    while (timer.elapsed() < timeoutMs)
    {
        if (parseBuffered())
            return true;
        if (process.bytesAvailable() > 0)
            buffer += process.readAllStandardOutput();
        if (parseBuffered())
            return true;
        if (!process.waitForReadyRead(100))
            continue;
        buffer += process.readAllStandardOutput();
    }

    if (errorMessage)
        *errorMessage =
            QStringLiteral("Error: timed out waiting for MCP response.");
    return false;
}

bool sendAndWait(QProcess &process, QByteArray &buffer, int requestId,
                 const json &request, int timeoutMs, json *result,
                 QString *errorMessage)
{
    if (process.write(encodeMessage(request)) < 0 ||
        !process.waitForBytesWritten(timeoutMs))
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Error: failed to write MCP request.");
        return false;
    }

    while (true)
    {
        json message;
        if (!readOneMessage(process, buffer, &message, timeoutMs, errorMessage))
            return false;
        if (!message.is_object())
            continue;
        if (message.contains("id") && message["id"].is_number_integer() &&
            message["id"].get<int>() == requestId)
        {
            if (message.contains("error"))
            {
                if (errorMessage)
                    *errorMessage =
                        QStringLiteral("Error: MCP error response: %1")
                            .arg(QString::fromStdString(
                                message["error"].dump()));
                return false;
            }
            *result = message.value("result", json::object());
            return true;
        }
    }
}

QString sanitizeToolIdPart(QString s)
{
    s.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")),
              QStringLiteral("_"));
    return s;
}

QString renderCallResult(const json &result)
{
    const bool isError = result.value("isError", false);
    QStringList chunks;
    if (result.contains("content") && result["content"].is_array())
    {
        for (const auto &entry : result["content"])
        {
            if (!entry.is_object())
                continue;
            if (entry.value("type", std::string()) == "text")
                chunks.append(
                    QString::fromStdString(entry.value("text", std::string())));
            else
                chunks.append(QString::fromStdString(entry.dump()));
        }
    }
    if (chunks.isEmpty() && result.contains("structuredContent"))
        chunks.append(
            QString::fromStdString(result["structuredContent"].dump(2)));
    if (chunks.isEmpty())
        chunks.append(QString::fromStdString(result.dump(2)));

    QString joined = chunks.join(QStringLiteral("\n\n")).trimmed();
    if (isError &&
        !joined.startsWith(QStringLiteral("Error:"), Qt::CaseInsensitive))
        joined.prepend(QStringLiteral("Error: "));
    return joined;
}

bool initializeServer(const McpTool::ServerConfig &config,
                      const QString &cwdOverride, QProcess &process,
                      QByteArray &buffer, QString *errorMessage)
{
    if (config.command.trimmed().isEmpty())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Error: MCP command is empty.");
        return false;
    }

    if (!config.env.isEmpty())
        process.setProcessEnvironment(config.env);
    const QString workdir =
        cwdOverride.trimmed().isEmpty() ? config.cwd : cwdOverride;
    if (!workdir.trimmed().isEmpty())
        process.setWorkingDirectory(workdir);
    process.start(config.command, config.args, QIODevice::ReadWrite);
    if (!process.waitForStarted(10000))
    {
        if (errorMessage)
            *errorMessage =
                QStringLiteral("Error: failed to start MCP server '%1': %2")
                    .arg(config.serverId, process.errorString());
        return false;
    }

    json ignored;
    json init = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params",
         {{"protocolVersion", "2024-11-05"},
          {"capabilities", json::object()},
          {"clientInfo", {{"name", "StarryAgent"}, {"version", "0.1.0"}}}}}};
    if (!sendAndWait(process, buffer, 1, init, config.timeoutMs, &ignored,
                     errorMessage))
        return false;

    const json initialized = {{"jsonrpc", "2.0"},
                              {"method", "notifications/initialized"},
                              {"params", json::object()}};
    if (process.write(encodeMessage(initialized)) < 0 ||
        !process.waitForBytesWritten(config.timeoutMs))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "Error: failed to send MCP initialized notification.");
        return false;
    }
    return true;
}
} // namespace

McpTool::McpTool(ServerConfig serverConfig, QString publicId,
                 QString remoteName, QString description, nlohmann::json schema)
    : m_serverConfig(std::move(serverConfig)), m_publicId(std::move(publicId)),
      m_remoteName(std::move(remoteName)),
      m_description(std::move(description)), m_schema(std::move(schema))
{
}

std::vector<std::unique_ptr<McpTool>>
McpTool::discover(const ServerConfig &config, QString *errorMessage)
{
    QProcess process;
    QByteArray buffer;
    if (!initializeServer(config, QString(), process, buffer, errorMessage))
        return {};

    json result;
    const json request = {{"jsonrpc", "2.0"},
                          {"id", 2},
                          {"method", "tools/list"},
                          {"params", json::object()}};
    if (!sendAndWait(process, buffer, 2, request, config.timeoutMs, &result,
                     errorMessage))
        return {};

    std::vector<std::unique_ptr<McpTool>> tools;
    if (result.contains("tools") && result["tools"].is_array())
    {
        for (const auto &tool : result["tools"])
        {
            if (!tool.is_object())
                continue;
            const QString remoteName =
                QString::fromStdString(tool.value("name", std::string()));
            if (remoteName.isEmpty())
                continue;
            const QString publicId =
                QStringLiteral("mcp__%1__%2")
                    .arg(sanitizeToolIdPart(config.serverId),
                         sanitizeToolIdPart(remoteName));
            const QString description = QString::fromStdString(
                tool.value("description", std::string()));
            const json schema =
                tool.value("inputSchema", json{{"type", "object"},
                                               {"properties", json::object()}});
            tools.push_back(std::make_unique<McpTool>(
                config, publicId, remoteName,
                description.isEmpty()
                    ? QStringLiteral("MCP tool %1 from server %2")
                          .arg(remoteName, config.serverId)
                    : description,
                schema));
        }
    }

    process.terminate();
    process.waitForFinished(1000);
    return tools;
}

QString McpTool::execute(const nlohmann::json &args)
{
    QProcess process;
    QByteArray buffer;
    QString error;
    if (!initializeServer(
            m_serverConfig,
            ToolWorkdir::effectiveWorkdir(args, m_serverConfig.cwd), process,
            buffer, &error))
        return error;

    json result;
    const json request = {
        {"jsonrpc", "2.0"},
        {"id", 3},
        {"method", "tools/call"},
        {"params",
         {{"name", m_remoteName.toStdString()}, {"arguments", args}}}};
    if (!sendAndWait(process, buffer, 3, request, m_serverConfig.timeoutMs,
                     &result, &error))
        return error;

    process.terminate();
    process.waitForFinished(1000);
    return renderCallResult(result);
}
