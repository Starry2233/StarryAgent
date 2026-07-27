#pragma once

#include <QColor>
#include <QString>

class Settings;

class ToastService
{
  public:
    static void bindSettings(Settings *settings);
    static bool showMessage(const QString &message);
    static void applyTheme(bool dark);

  private:
    static void applyDesktopColors(bool dark);
};
