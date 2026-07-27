#pragma once

#include <QNetworkAccessManager>
#include <QObject>

class ImageTransferService : public QObject
{
    Q_OBJECT
  public:
    explicit ImageTransferService(QObject *parent = nullptr);

    Q_INVOKABLE void download(const QString &source,
                              const QString &suggestedName = QString());

  signals:
    void downloadFinished(const QString &source, const QString &savedPath,
                          const QString &error);

  private:
    QNetworkAccessManager m_network;
};
