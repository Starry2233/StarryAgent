#pragma once

#include <QtCore/qglobal.h>

class QWindow;
class QWidget;

namespace AppWindowChrome
{
void applyToWindow(QWindow *window, bool dark);
void applyToWidget(QWidget *widget, bool dark);
}
