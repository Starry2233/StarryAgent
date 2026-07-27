#include "ToastService.h"

#include "core/Settings.h"

#include <QObject>

#ifdef Q_OS_ANDROID
#include <QtCore/QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qnativeinterface.h>
#else
#include "msgtoast.h"
#endif

namespace
{
bool s_darkTheme = false;
}

void ToastService::bindSettings(Settings *settings)
{
    if (!settings)
        return;
    applyTheme(settings->theme() == QStringLiteral("dark"));
    QObject::connect(
        settings, &Settings::themeChanged, settings, [settings]
        { applyTheme(settings->theme() == QStringLiteral("dark")); });
}

void ToastService::applyTheme(bool dark)
{
    s_darkTheme = dark;
#ifndef Q_OS_ANDROID
    applyDesktopColors(dark);
#endif
}

bool ToastService::showMessage(const QString &message)
{
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
        [context, message]
        {
            QJniObject text = QJniObject::fromString(message);
            QJniObject toast = QJniObject::callStaticObjectMethod(
                "android/widget/Toast", "makeText",
                "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/"
                "widget/Toast;",
                context.object(), text.object<jstring>(), jint(0));
            if (toast.isValid())
                toast.callMethod<void>("show");
        });
    return true;
#else
    applyDesktopColors(s_darkTheme);
    Toast::showMsg(message, ToastTime::ToastTime_short);
    return true;
#endif
}

#ifndef Q_OS_ANDROID
void ToastService::applyDesktopColors(bool dark)
{
    if (dark)
    {
        Toast::setColors(QColor(QStringLiteral("#221E19")),
                         QColor(QStringLiteral("#E8E1D0")),
                         QColor(QStringLiteral("#4A4035")));
    }
    else
    {
        Toast::setColors(QColor(QStringLiteral("#FBF6EC")),
                         QColor(QStringLiteral("#1C1916")),
                         QColor(QStringLiteral("#D9CFBC")));
    }
}
#endif
