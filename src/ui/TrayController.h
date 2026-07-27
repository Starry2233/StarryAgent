#pragma once

#include <QObject>

class QSystemTrayIcon;
class QWindow;
class QApplication;
class Settings;

class TrayController : public QObject
{
    Q_OBJECT

  public:
    explicit TrayController(Settings *settings, QObject *parent = nullptr);
    void attach(QApplication *app, QWindow *window);
    bool supported() const;
    void showSystemNotification(const QString &title, const QString &message);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void updateEnabledState();
    void restoreWindow();
    void exitApplication();

    Settings *m_settings = nullptr;
    QApplication *m_app = nullptr;
    QWindow *m_window = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    bool m_exiting = false;
};
