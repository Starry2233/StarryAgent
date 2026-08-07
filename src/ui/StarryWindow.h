#pragma once

#include <QtCore/qglobal.h>
#include <QQuickWindow>

class QEvent;
class QWindow;

class StarryWindow : public QQuickWindow
{
    Q_OBJECT

  public:
    explicit StarryWindow(QWindow *parent = nullptr);

  protected:
    bool event(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

  private:
    void applyChrome();
#ifdef Q_OS_ANDROID
    bool m_androidHiddenForBackground = false;
#endif
};
