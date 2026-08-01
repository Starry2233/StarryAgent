#pragma once

#include <QObject>

class AndroidBackgroundRuntime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool batteryOptimizationIgnored READ batteryOptimizationIgnored
                   NOTIFY batteryOptimizationIgnoredChanged)

  public:
    explicit AndroidBackgroundRuntime(QObject *parent = nullptr);

    bool batteryOptimizationIgnored() const;

    Q_INVOKABLE void refreshBatteryOptimizationState();
    Q_INVOKABLE bool requestIgnoreBatteryOptimizations();
    Q_INVOKABLE bool openBackgroundSettings();

  signals:
    void batteryOptimizationIgnoredChanged();
    void errorOccurred(const QString &message);
    void requestLaunched(const QString &message);

  private:
    bool setBatteryOptimizationIgnored(bool ignored);

    bool m_batteryOptimizationIgnored = false;
};
