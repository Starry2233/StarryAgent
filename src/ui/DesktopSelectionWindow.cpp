#include "DesktopSelectionWindow.h"

#ifndef Q_OS_ANDROID

#include "AppWindowChrome.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QWidget>

DesktopSelectionWindow::DesktopSelectionWindow(QObject *parent)
    : QObject(parent)
{
}

DesktopSelectionWindow::~DesktopSelectionWindow()
{
    delete m_window;
    m_window = nullptr;
    m_titleLabel = nullptr;
    m_editor = nullptr;
    m_copyButton = nullptr;
}

void DesktopSelectionWindow::openText(const QString &title, const QString &text,
                                      bool dark)
{
    ensureWindow();
    m_titleLabel->setText(title);
    m_window->setWindowTitle(title.isEmpty() ? QStringLiteral("Select Text")
                                             : title);
    applyTheme(dark);
    m_editor->setPlainText(text);
    m_editor->moveCursor(QTextCursor::Start);
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void DesktopSelectionWindow::ensureWindow()
{
    if (m_window)
        return;

    m_window = new QWidget();
    m_window->setAttribute(Qt::WA_DeleteOnClose, false);
    m_window->setWindowTitle(QStringLiteral("Select Text"));
    m_window->resize(900, 640);

    auto *layout = new QVBoxLayout(m_window);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout();
    header->setSpacing(8);
    m_titleLabel = new QLabel(m_window);
    header->addWidget(m_titleLabel, 1);

    m_copyButton = new QPushButton(QStringLiteral("Copy"), m_window);
    QObject::connect(m_copyButton, &QPushButton::clicked, m_window,
                     [this]()
                     {
                         if (QClipboard *clipboard = QApplication::clipboard())
                             clipboard->setText(m_editor
                                                    ? m_editor->toPlainText()
                                                    : QString());
                     });
    header->addWidget(m_copyButton, 0);
    layout->addLayout(header);

    m_editor = new QPlainTextEdit(m_window);
    m_editor->setReadOnly(true);
    m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(m_editor, 1);

    AppWindowChrome::applyToWidget(m_window, false);
    applyTheme(false);
}

void DesktopSelectionWindow::applyTheme(bool dark)
{
    if (!m_window)
        return;

    const QString paper =
        dark ? QStringLiteral("#1A1714") : QStringLiteral("#F5EFE3");
    const QString surface =
        dark ? QStringLiteral("#221E19") : QStringLiteral("#FBF6EC");
    const QString ink =
        dark ? QStringLiteral("#E8E1D0") : QStringLiteral("#1C1916");
    const QString inkSoft =
        dark ? QStringLiteral("#948A79") : QStringLiteral("#6B6357");
    const QString line =
        dark ? QStringLiteral("#3B342C") : QStringLiteral("#D9CFBC");
    const QString clay =
        dark ? QStringLiteral("#D2693A") : QStringLiteral("#C2502A");
    const QString clayDeep =
        dark ? QStringLiteral("#B0542E") : QStringLiteral("#9E3D1F");

    m_window->setStyleSheet(
        QStringLiteral(R"(
QWidget {
    background: %1;
    color: %2;
    font-family: "Hanken Grotesk", "Microsoft YaHei UI", sans-serif;
    font-size: 14px;
}
QLabel {
    color: %2;
    font-size: 14px;
    font-weight: 600;
}
QPushButton {
    background: %3;
    color: white;
    border: 1px solid %5;
    border-radius: 8px;
    padding: 6px 12px;
    min-width: 56px;
}
QPushButton:hover {
    background: %4;
}
QPushButton:pressed {
    background: %5;
}
QPlainTextEdit {
    background: %6;
    color: %2;
    border: 1px solid %7;
    border-radius: 12px;
    padding: 10px;
    selection-background-color: %3;
    selection-color: white;
    font-family: "Hanken Grotesk", "Microsoft YaHei UI", sans-serif;
    font-size: 14px;
}
QScrollBar:vertical {
    background: transparent;
    width: 12px;
    margin: 6px 2px 6px 2px;
}
QScrollBar::handle:vertical {
    background: %7;
    border-radius: 5px;
    min-height: 28px;
}
QScrollBar::handle:vertical:hover {
    background: %8;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: transparent;
    height: 0px;
}
    )")
            .arg(paper, ink, clay, clayDeep, clayDeep, surface, line, inkSoft));
}

#else

DesktopSelectionWindow::DesktopSelectionWindow(QObject *parent)
    : QObject(parent)
{
}

DesktopSelectionWindow::~DesktopSelectionWindow() = default;

void DesktopSelectionWindow::openText(const QString &, const QString &, bool) {}

void DesktopSelectionWindow::applyTheme(bool) {}

#endif
