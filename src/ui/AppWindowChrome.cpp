#include "AppWindowChrome.h"

#include <QWindow>
#ifndef Q_OS_ANDROID
#include <QWidget>
#endif

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace AppWindowChrome
{
#ifdef Q_OS_WIN
namespace
{
void ensureDialogModalFrame(HWND hwnd)
{
    if (!hwnd)
        return;

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR wantedStyle = exStyle | WS_EX_DLGMODALFRAME;
    if (wantedStyle != exStyle)
    {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, wantedStyle);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                         SWP_FRAMECHANGED);
    }
}

void applyDarkTitleBar(HWND hwnd, bool dark)
{
    if (!hwnd)
        return;

    const BOOL enabled = dark ? TRUE : FALSE;
    HRESULT hr =
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                              &enabled, sizeof(enabled));
    if (FAILED(hr))
    {
        DwmSetWindowAttribute(
            hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &enabled,
            sizeof(enabled));
    }
}

void clearWindowClassIcon(HWND hwnd)
{
    if (!hwnd)
        return;
    SetClassLongPtrW(hwnd, GCLP_HICON, 0);
    SetClassLongPtrW(hwnd, GCLP_HICONSM, 0);
}
}
#endif

void applyToWindow(QWindow *window, bool dark)
{
    if (!window)
        return;

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    ensureDialogModalFrame(hwnd);
    applyDarkTitleBar(hwnd, dark);
    clearWindowClassIcon(hwnd);
#else
    Q_UNUSED(window);
    Q_UNUSED(dark);
#endif
}

void applyToWidget(QWidget *widget, bool dark)
{
    if (!widget)
        return;

#ifdef Q_OS_WIN
    widget->winId();
    if (QWindow *window = widget->windowHandle())
    {
        applyToWindow(window, dark);
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(widget->effectiveWinId());
    ensureDialogModalFrame(hwnd);
    applyDarkTitleBar(hwnd, dark);
    clearWindowClassIcon(hwnd);
#else
    Q_UNUSED(widget);
    Q_UNUSED(dark);
#endif
}
}
