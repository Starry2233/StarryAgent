#pragma once

#include <QObject>

class Settings;

class AutoStartManager : public QObject
{
    Q_OBJECT

  public:
    explicit AutoStartManager(Settings *settings, QObject *parent = nullptr);

    bool supported() const;

  private:
    void apply(bool enabled);
    static QString appDisplayName();
};
