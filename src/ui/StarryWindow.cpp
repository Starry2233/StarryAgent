#include "StarryWindow.h"

#include "AppWindowChrome.h"

#include <QEvent>

#ifdef Q_OS_ANDROID
#include <QtCore/QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qnativeinterface.h>
#endif

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

StarryWindow::StarryWindow(QWindow *parent)
    : QQuickWindow(parent)
{
}

#ifdef Q_OS_WIN
bool StarryWindow::nativeEvent(const QByteArray &eventType, void *message,
                               qintptr *result)
{
    if (eventType != QStringLiteral("windows_generic_MSG"))
        return QQuickWindow::nativeEvent(eventType, message, result);

    MSG *msg = static_cast<MSG *>(message);
    if (msg->message == WM_NCCREATE)
    {
        CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(msg->lParam);
        if (cs)
            cs->dwExStyle |= WS_EX_DLGMODALFRAME;
    }
    return QQuickWindow::nativeEvent(eventType, message, result);
}
#else
bool StarryWindow::nativeEvent(const QByteArray &eventType, void *message,
                               qintptr *result)
{
    return QQuickWindow::nativeEvent(eventType, message, result);
}
#endif

bool StarryWindow::event(QEvent *event)
{
#ifdef Q_OS_ANDROID
    if (event->type() == QEvent::Close)
    {
        event->ignore();
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid())
            activity.callMethod<void>("handleQtWindowClose", "()V");
        return true;
    }

    switch (event->type())
    {
    case QEvent::Hide:
        m_androidHiddenForBackground = true;
        break;
    case QEvent::Show:
        m_androidHiddenForBackground = false;
        break;
    case QEvent::UpdateRequest:
        if (m_androidHiddenForBackground)
            return true;
        break;
    default:
        break;
    }
#endif

    const bool handled = QQuickWindow::event(event);
    switch (event->type())
    {
    case QEvent::Show:
        applyChrome();
        break;
    case QEvent::PlatformSurface:
    case QEvent::PaletteChange:
    case QEvent::WinIdChange:
        applyChrome();
        break;
    default:
        break;
    }
    return handled;
}

void StarryWindow::applyChrome()
{
    const bool dark = color().lightness() < 128;
    AppWindowChrome::applyToWindow(this, dark);
}
