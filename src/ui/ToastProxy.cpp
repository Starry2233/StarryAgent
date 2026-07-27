#include "ToastProxy.h"

#include "ToastService.h"

ToastProxy::ToastProxy(QObject *parent) : QObject(parent) {}

bool ToastProxy::showMessage(const QString &message) const
{
    return ToastService::showMessage(message);
}

bool ToastProxy::forwardMessage(const QString &message)
{
    emit messageRequested(message);
    return true;
}
