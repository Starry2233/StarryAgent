#include "ClipboardProxy.h"
#include "ToastService.h"

#include <QClipboard>
#include <QGuiApplication>

ClipboardProxy::ClipboardProxy(QObject *parent) : QObject(parent) {}

void ClipboardProxy::setText(const QString &text)
{
    if (QClipboard *cb = QGuiApplication::clipboard())
        cb->setText(text);
}

QString ClipboardProxy::text() const
{
    if (QClipboard *cb = QGuiApplication::clipboard())
        return cb->text();
    return {};
}

bool ClipboardProxy::showCopyFeedback()
{
    return ToastService::showMessage(QStringLiteral("Copied"));
}
