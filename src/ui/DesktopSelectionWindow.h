#pragma once

#include <QObject>

class QWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;

class DesktopSelectionWindow : public QObject
{
    Q_OBJECT
  public:
    explicit DesktopSelectionWindow(QObject *parent = nullptr);
    ~DesktopSelectionWindow() override;

    Q_INVOKABLE void openText(const QString &title, const QString &text,
                              bool dark = false);

  private:
    void ensureWindow();
    void applyTheme(bool dark);

    QWidget *m_window = nullptr;
    QLabel *m_titleLabel = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QPushButton *m_copyButton = nullptr;
};
