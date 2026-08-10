#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class QJsonObject;

namespace StarryAgent
{
class BackendProcessTransport : public QObject
{
    Q_OBJECT

  public:
    explicit BackendProcessTransport(QObject *parent = nullptr);

    void setProgram(const QString &program);
    void setArguments(const QStringList &arguments);
    QString program() const { return m_program; }
    QStringList arguments() const { return m_arguments; }
    bool isRunning() const;

    bool start();
    void stop();
    bool sendMessage(const QJsonObject &message);

  signals:
    void started();
    void stopped();
    void messageReceived(const QJsonObject &message);
    void errorOccurred(const QString &message);
    void stderrReceived(const QString &text);

  private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

  private:
    void consumeStdoutBuffer();
    void emitProtocolError(const QString &message);

    QProcess m_process;
    QString m_program;
    QStringList m_arguments;
    QByteArray m_stdoutBuffer;
};
} // namespace StarryAgent
