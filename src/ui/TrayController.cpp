#include "TrayController.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QWindow>

#include "core/Settings.h"

TrayController::TrayController(Settings *settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    if (!m_settings)
        return;
    connect(m_settings, &Settings::closeToTrayChanged, this,
            &TrayController::updateEnabledState);
}

void TrayController::attach(QApplication *app, QWindow *window)
{
    m_app = app;
    m_window = window;
    if (!m_app || !m_window)
        return;

    m_window->installEventFilter(this);
    if (!supported())
        return;

    auto *menu = new QMenu();
    QAction *showAction = menu->addAction(QStringLiteral("Show StarryAgent"));
    QAction *quitAction = menu->addAction(QStringLiteral("Exit"));
    connect(showAction, &QAction::triggered, this,
            &TrayController::restoreWindow);
    connect(quitAction, &QAction::triggered, this,
            &TrayController::exitApplication);

    m_trayIcon = new QSystemTrayIcon(this);
    const QIcon icon =
        m_app->windowIcon().isNull()
            ? m_app->style()->standardIcon(QStyle::SP_ComputerIcon)
            : m_app->windowIcon();
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(QStringLiteral("StarryAgent"));
    m_trayIcon->setContextMenu(menu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick)
                    restoreWindow();
            });
    updateEnabledState();
}

bool TrayController::supported() const
{
#if defined(Q_OS_ANDROID)
    return false;
#else
    return QSystemTrayIcon::isSystemTrayAvailable();
#endif
}

void TrayController::showSystemNotification(const QString &title,
                                            const QString &message)
{
    if (!m_trayIcon || !supported())
        return;
    if (!m_trayIcon->isVisible())
        m_trayIcon->show();
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information,
                            10000);
}

bool TrayController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event && event->type() == QEvent::Close &&
        !m_exiting && supported() && m_settings &&
        m_settings->closeToTray())
    {
        auto *closeEvent = static_cast<QCloseEvent *>(event);
        closeEvent->ignore();
        if (m_window)
            m_window->hide();
        if (m_trayIcon)
        {
            m_trayIcon->show();
            m_trayIcon->showMessage(
                QStringLiteral("StarryAgent"),
                QStringLiteral("StarryAgent is still running in the tray."));
        }
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void TrayController::updateEnabledState()
{
    if (!m_trayIcon)
        return;
    const bool enabled =
        supported() && m_settings && m_settings->closeToTray();
    if (enabled)
    {
        m_trayIcon->show();
        if (m_app)
            m_app->setQuitOnLastWindowClosed(false);
    }
    else
    {
        m_trayIcon->hide();
        if (m_app)
            m_app->setQuitOnLastWindowClosed(true);
    }
}

void TrayController::restoreWindow()
{
    if (!m_window)
        return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void TrayController::exitApplication()
{
    m_exiting = true;
    if (m_app)
    {
        m_app->setQuitOnLastWindowClosed(true);
        m_app->quit();
    }
}
