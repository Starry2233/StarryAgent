#include "ipc/BackendWebSocketTransport.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace StarryAgent
{
BackendWebSocketTransport::BackendWebSocketTransport(QObject *parent)
    : IBackendTransport(parent)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(1500);

    connect(&m_socket, &QWebSocket::connected, this,
            &BackendWebSocketTransport::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this,
            &BackendWebSocketTransport::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this,
            &BackendWebSocketTransport::onTextMessageReceived);
    connect(&m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &BackendWebSocketTransport::onSocketError);
    connect(&m_socket, &QWebSocket::stateChanged, this,
            &BackendWebSocketTransport::onStateChanged);
    connect(&m_reconnectTimer, &QTimer::timeout, this,
            &BackendWebSocketTransport::reconnectNow);
}

void BackendWebSocketTransport::setUrl(const QUrl &url)
{
    m_url = url;
}

void BackendWebSocketTransport::setBearerToken(const QString &token)
{
    m_bearerToken = token;
}

void BackendWebSocketTransport::setAutoReconnect(bool enabled)
{
    m_autoReconnect = enabled;
}

bool BackendWebSocketTransport::isRunning() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool BackendWebSocketTransport::start()
{
    if (!m_url.isValid() || m_url.isEmpty())
        return false;
    if (m_socket.state() == QAbstractSocket::ConnectedState ||
        m_socket.state() == QAbstractSocket::ConnectingState)
        return false;

    m_userInitiatedStop = false;
    m_shouldReconnect = m_autoReconnect;
    openSocket();
    return true;
}

void BackendWebSocketTransport::stop()
{
    m_userInitiatedStop = true;
    m_shouldReconnect = false;
    m_reconnectTimer.stop();
    if (m_socket.state() == QAbstractSocket::UnconnectedState)
        return;
    m_socket.close();
}

bool BackendWebSocketTransport::sendMessage(const QJsonObject &message)
{
    if (!isRunning())
        return false;
    const QString payload = QString::fromUtf8(
        QJsonDocument(message).toJson(QJsonDocument::Compact));
    return m_socket.sendTextMessage(payload) == payload.size();
}

void BackendWebSocketTransport::onConnected()
{
    m_reconnectTimer.stop();
    m_wasConnected = true;
    emit started();
}

void BackendWebSocketTransport::onDisconnected()
{
    const bool wasConnected = m_wasConnected;
    m_wasConnected = false;
    emit stopped();
    if (!m_userInitiatedStop && m_shouldReconnect)
    {
        scheduleReconnect();
        if (wasConnected)
        {
            emit errorOccurred(
                QStringLiteral("backend websocket disconnected; reconnecting"));
        }
    }
}

void BackendWebSocketTransport::onTextMessageReceived(const QString &message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
    {
        emitProtocolError(QStringLiteral("invalid backend websocket json"));
        return;
    }
    emit messageReceived(doc.object());
}

void BackendWebSocketTransport::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_socket.errorString());
}

void BackendWebSocketTransport::onStateChanged(QAbstractSocket::SocketState state)
{
    if (state != QAbstractSocket::UnconnectedState)
        return;
    if (m_userInitiatedStop || !m_shouldReconnect || m_wasConnected)
        return;
    scheduleReconnect();
}

void BackendWebSocketTransport::reconnectNow()
{
    if (m_userInitiatedStop || !m_shouldReconnect)
        return;
    openSocket();
}

void BackendWebSocketTransport::openSocket()
{
    QNetworkRequest request(m_url);
    if (!m_bearerToken.trimmed().isEmpty())
    {
        request.setRawHeader(
            "Authorization",
            QStringLiteral("Bearer %1").arg(m_bearerToken).toUtf8());
    }
    m_socket.open(request);
}

void BackendWebSocketTransport::scheduleReconnect()
{
    if (m_reconnectTimer.isActive())
        return;
    m_reconnectTimer.start();
}

void BackendWebSocketTransport::emitProtocolError(const QString &message)
{
    emit errorOccurred(message);
}

} // namespace StarryAgent
