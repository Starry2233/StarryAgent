#include "AndroidBackgroundRuntime.h"

#ifdef Q_OS_ANDROID
#include <QPointer>

#include <QtCore/QJniObject>
#include <QtCore/private/qandroidextras_p.h>
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qnativeinterface.h>
#endif

namespace
{
#ifdef Q_OS_ANDROID
constexpr int kIgnoreBatteryOptimizationsRequestCode = 0x5344;
constexpr int kBackgroundSettingsRequestCode = 0x5345;

QJniObject androidContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

QJniObject packageName(const QJniObject &context)
{
    return context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
}

bool isIgnoringBatteryOptimizations(const QJniObject &context)
{
    if (!context.isValid())
        return false;
    const QJniObject serviceName =
        QJniObject::getStaticObjectField("android/content/Context",
                                         "POWER_SERVICE",
                                         "Ljava/lang/String;");
    const QJniObject powerManager = context.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        serviceName.object<jstring>());
    if (!powerManager.isValid())
        return false;
    const QJniObject pkg = packageName(context);
    if (!pkg.isValid())
        return false;
    return powerManager.callMethod<jboolean>(
        "isIgnoringBatteryOptimizations", "(Ljava/lang/String;)Z",
        pkg.object<jstring>());
}

QJniObject packageUri(const QJniObject &context)
{
    const QJniObject scheme = QJniObject::fromString(QStringLiteral("package"));
    const QJniObject pkg = packageName(context);
    return QJniObject::callStaticObjectMethod(
        "android/net/Uri", "fromParts",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;",
        scheme.object<jstring>(), pkg.object<jstring>(), nullptr);
}
#endif
} // namespace

AndroidBackgroundRuntime::AndroidBackgroundRuntime(QObject *parent)
    : QObject(parent)
{
    refreshBatteryOptimizationState();
}

bool AndroidBackgroundRuntime::batteryOptimizationIgnored() const
{
    return m_batteryOptimizationIgnored;
}

void AndroidBackgroundRuntime::refreshBatteryOptimizationState()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = androidContext();
    setBatteryOptimizationIgnored(isIgnoringBatteryOptimizations(context));
#else
    setBatteryOptimizationIgnored(false);
#endif
}

bool AndroidBackgroundRuntime::requestIgnoreBatteryOptimizations()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = androidContext();
    if (!context.isValid())
    {
        emit errorOccurred(QStringLiteral("Android context unavailable"));
        return false;
    }
    setBatteryOptimizationIgnored(isIgnoringBatteryOptimizations(context));
    if (m_batteryOptimizationIgnored)
    {
        emit requestLaunched(QStringLiteral(
            "Battery optimization is already disabled for StarryAgent."));
        return true;
    }

    const QJniObject action = QJniObject::fromString(
        QStringLiteral("android.settings.REQUEST_IGNORE_BATTERY_OPTIMIZATIONS"));
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      action.object<jstring>());
    if (!intent.isValid())
    {
        emit errorOccurred(
            QStringLiteral("Failed to create battery optimization request"));
        return false;
    }

    const QJniObject uri = packageUri(context);
    if (uri.isValid())
    {
        intent.callObjectMethod("setData",
                                "(Landroid/net/Uri;)Landroid/content/Intent;",
                                uri.object());
    }

    QPointer<AndroidBackgroundRuntime> guard(this);
    QtAndroidPrivate::startActivity(
        intent, kIgnoreBatteryOptimizationsRequestCode,
        [guard](int requestCode, int, const QJniObject &)
        {
            if (!guard || requestCode != kIgnoreBatteryOptimizationsRequestCode)
                return;
            guard->refreshBatteryOptimizationState();
        });
    emit requestLaunched(QStringLiteral(
        "Opened Android battery optimization request. Allow unrestricted background running if prompted."));
    return true;
#else
    emit errorOccurred(QStringLiteral("Android only"));
    return false;
#endif
}

bool AndroidBackgroundRuntime::openBackgroundSettings()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = androidContext();
    if (!context.isValid())
    {
        emit errorOccurred(QStringLiteral("Android context unavailable"));
        return false;
    }

    const QJniObject action = QJniObject::fromString(
        QStringLiteral("android.settings.APPLICATION_DETAILS_SETTINGS"));
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      action.object<jstring>());
    if (!intent.isValid())
    {
        emit errorOccurred(QStringLiteral("Failed to open app settings"));
        return false;
    }

    const QJniObject uri = packageUri(context);
    if (uri.isValid())
    {
        intent.callObjectMethod("setData",
                                "(Landroid/net/Uri;)Landroid/content/Intent;",
                                uri.object());
    }

    QPointer<AndroidBackgroundRuntime> guard(this);
    QtAndroidPrivate::startActivity(
        intent, kBackgroundSettingsRequestCode,
        [guard](int requestCode, int, const QJniObject &)
        {
            if (!guard || requestCode != kBackgroundSettingsRequestCode)
                return;
            guard->refreshBatteryOptimizationState();
        });
    emit requestLaunched(QStringLiteral(
        "Opened StarryAgent app settings. Check battery, startup, and background permissions there if needed."));
    return true;
#else
    emit errorOccurred(QStringLiteral("Android only"));
    return false;
#endif
}

bool AndroidBackgroundRuntime::moveTaskToBack()
{
#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid())
        return false;
    return activity.callMethod<jboolean>("moveTaskToBack", "(Z)Z", jboolean(true));
#else
    return false;
#endif
}

bool AndroidBackgroundRuntime::setBatteryOptimizationIgnored(bool ignored)
{
    if (m_batteryOptimizationIgnored == ignored)
        return false;
    m_batteryOptimizationIgnored = ignored;
    emit batteryOptimizationIgnoredChanged();
    return true;
}
