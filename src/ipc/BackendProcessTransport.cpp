#include "ipc/BackendProcessTransport.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace StarryAgent
{
BackendProcessTransport::BackendProcessTransport(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this,
            &BackendProcessTransport::onReadyReadStandardOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this,
            &BackendProcessTransport::onReadyReadStandardError);
    connect(&m_process, &QProcess::errorOccurred, this,
            &BackendProcessTransport::onProcessError);
    connect(&m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &BackendProcessTransport::onProcessFinished);
}

void BackendProcessTransport::setProgram(const QString &program)
{
    m_program = program;
}

void BackendProcessTransport::setArguments(const QStringList &arguments)
{
    m_arguments = arguments;
}

bool BackendProcessTransport::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

bool BackendProcessTransport::start()
{
    if (m_program.isEmpty() || isRunning())
        return false;
    m_process.setProgram(m_program);
    m_process.setArguments(m_arguments);
    m_process.start();
    const bool started = m_process.waitForStarted();
    if (started)
        emit this->started();
    return started;
}

void BackendProcessTransport::stop()
{
    if (!isRunning())
        return;
    m_process.closeWriteChannel();
    m_process.terminate();
    if (!m_process.waitForFinished(1500))
        m_process.kill();
}

bool BackendProcessTransport::sendMessage(const QJsonObject &message)
{
    if (!isRunning())
        return false;
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    return m_process.write(payload) == payload.size();
}

void BackendProcessTransport::onReadyReadStandardOutput()
{
    m_stdoutBuffer.append(m_process.readAllStandardOutput());
    consumeStdoutBuffer();
}

void BackendProcessTransport::onReadyReadStandardError()
{
    const QByteArray data = m_process.readAllStandardError();
    if (!data.isEmpty())
        emit stderrReceived(QString::fromUtf8(data));
}

void BackendProcessTransport::onProcessError(QProcess::ProcessError error)
{
    emit errorOccurred(QStringLiteral("backend process error %1").arg(int(error)));
}

void BackendProcessTransport::onProcessFinished(int, QProcess::ExitStatus)
{
    emit stopped();
}

void BackendProcessTransport::consumeStdoutBuffer()
{
    while (true)
    {
        const int newline = m_stdoutBuffer.indexOf('\n');
        if (newline < 0)
            break;
        const QByteArray line = m_stdoutBuffer.left(newline).trimmed();
        m_stdoutBuffer.remove(0, newline + 1);
        if (line.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
        {
            emitProtocolError(QStringLiteral("invalid backend json line"));
            continue;
        }
        emit messageReceived(doc.object());
    }
}

void BackendProcessTransport::emitProtocolError(const QString &message)
{
    emit errorOccurred(message);
}

} // namespace StarryAgent
