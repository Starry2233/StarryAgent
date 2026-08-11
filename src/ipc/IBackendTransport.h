#pragma once

#include <QObject>

class QJsonObject;

namespace StarryAgent
{
class IBackendTransport : public QObject
{
    Q_OBJECT

  public:
    using QObject::QObject;
    ~IBackendTransport() override = default;

    virtual bool isRunning() const = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool sendMessage(const QJsonObject &message) = 0;

  signals:
    void started();
    void stopped();
    void messageReceived(const QJsonObject &message);
    void errorOccurred(const QString &message);
};
} // namespace StarryAgent
