#pragma once

#include "ipc/IBackendTransport.h"

#include <QTimer>
#include <QUrl>
#include <QWebSocket>

namespace StarryAgent
{
class BackendWebSocketTransport : public IBackendTransport
{
    Q_OBJECT

  public:
    explicit BackendWebSocketTransport(QObject *parent = nullptr);

    void setUrl(const QUrl &url);
    QUrl url() const { return m_url; }

    void setBearerToken(const QString &token);
    QString bearerToken() const { return m_bearerToken; }

    void setAutoReconnect(bool enabled);
    bool autoReconnect() const { return m_autoReconnect; }

    bool isRunning() const override;
    bool start() override;
    void stop() override;
    bool sendMessage(const QJsonObject &message) override;

  private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onSocketError(QAbstractSocket::SocketError error);
    void onStateChanged(QAbstractSocket::SocketState state);
    void reconnectNow();

  private:
    void openSocket();
    void scheduleReconnect();
    void emitProtocolError(const QString &message);

    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QUrl m_url;
    QString m_bearerToken;
    bool m_autoReconnect = false;
    bool m_shouldReconnect = false;
    bool m_userInitiatedStop = false;
    bool m_wasConnected = false;
};
} // namespace StarryAgent
