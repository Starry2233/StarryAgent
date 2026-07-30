#include <QtCore/qglobal.h>
#ifdef Q_OS_ANDROID
#include <QGuiApplication>
#else
#include <QApplication>
#endif
#include <QEventLoop>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

#include "api/pipeline_smoke.h"
#include "chat/ConversationManager.h"
#include "chat/ScheduledTaskManager.h"
#include "core/Config.h"
#include "core/AutoStartManager.h"
#include "core/DebugTrace.h"
#include "core/ProcessMemoryLimiter.h"
#include "core/Settings.h"
#include "tools/ToolRegistry.h"
#include "theme/ThemeManager.h"
#include "tools/tools_smoke.h"
#include "ui/CameraBridge.h"
#include "ui/ClipboardProxy.h"
#include "ui/CodeHighlighter.h"
#include "ui/DesktopSelectionWindow.h"
#include "ui/FilePicker.h"
#include "ui/ImageTransferService.h"
#include "ui/MarkdownParser.h"
#include "ui/TrayController.h"
#include "ui/ToastProxy.h"
#include "ui/ToastService.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#include <cstdio>
#include <iostream>

#ifdef Q_OS_WIN
namespace
{
void attachWindowsParentConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS) && GetLastError() != ERROR_ACCESS_DENIED)
        return;

    FILE *stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    std::cout.clear();
    std::cerr.clear();
}

void applyWindowsDarkTitleBar(QWindow *window, bool dark)
{
    if (!window)
        return;

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;

    const BOOL enabled = dark ? TRUE : FALSE;
    // Windows 11 / newer Windows 10 builds.
    HRESULT hr =
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                              &enabled, sizeof(enabled));
    if (FAILED(hr))
    {
        // Older Windows 10 builds used 19.
        DwmSetWindowAttribute(
            hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &enabled,
            sizeof(enabled));
    }
}

void applyThemeToTopLevelWindows(const QList<QObject *> &objects, bool dark)
{
    for (QObject *object : objects)
    {
        if (auto *window = qobject_cast<QWindow *>(object))
            applyWindowsDarkTitleBar(window, dark);
    }
}
} // namespace
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    attachWindowsParentConsole();
#endif
    QStringList rawArgs;
    int maxRenderPageSize = 420;
    QString requestedRhi;
    for (int idx = 1; idx < argc; ++idx)
        rawArgs.append(QString::fromLocal8Bit(argv[idx]));

    DebugTrace::configureFromArgs(rawArgs);
    DebugTrace::applyVerboseQtEnvironment();
    DebugTrace::installMessageHandler();

    for (int idx = 1; idx < argc; ++idx)
    {
        const QString arg = QString::fromLocal8Bit(argv[idx]);
        QString value;
        if (arg == QStringLiteral("--max-old-space-size") && idx + 1 < argc)
        {
            value = QString::fromLocal8Bit(argv[++idx]);
        }
        else if (arg.startsWith(QStringLiteral("--max-old-space-size=")))
        {
            value = arg.mid(QStringLiteral("--max-old-space-size=").size());
        }
        else
        {
            continue;
        }
        bool ok = false;
        const int megabytes = value.toInt(&ok);
        if (!ok || megabytes <= 0)
        {
            QTextStream(stderr)
                << "Invalid value for --max-old-space-size: " << value
                << Qt::endl;
            return 2;
        }

        QString error;
        if (!ProcessMemoryLimiter::applyMegabytes(megabytes, &error))
        {
            QTextStream(stderr)
                << "Failed to apply --max-old-space-size=" << megabytes
                << "MB: " << error << Qt::endl;
            return 2;
        }
        qInfo("Applied process memory limit: %d MB", megabytes);
    }
    for (int idx = 0; idx < rawArgs.size(); ++idx)
    {
        const QString &arg = rawArgs.at(idx);
        QString value;
        if (arg == QStringLiteral("--rhi") && idx + 1 < rawArgs.size())
        {
            requestedRhi = rawArgs.at(++idx).trimmed().toLower();
            continue;
        }
        else if (arg.startsWith(QStringLiteral("--rhi=")))
        {
            requestedRhi =
                arg.mid(QStringLiteral("--rhi=").size()).trimmed().toLower();
            continue;
        }
        else if (arg == QStringLiteral("--max-render-page-size") &&
                 idx + 1 < rawArgs.size())
        {
            value = rawArgs.at(++idx);
        }
        else if (arg.startsWith(QStringLiteral("--max-render-page-size=")))
        {
            value = arg.mid(QStringLiteral("--max-render-page-size=").size());
        }
        else
        {
            continue;
        }
        bool ok = false;
        const int pixels = value.toInt(&ok);
        if (!ok || pixels <= 0)
        {
            QTextStream(stderr)
                << "Invalid value for --max-render-page-size: " << value
                << Qt::endl;
            return 2;
        }
        maxRenderPageSize = pixels;
    }

    auto applyRhiBackend = [](const QString &backend) -> bool
    {
        if (backend.isEmpty())
            return false;
        if (backend != QStringLiteral("d3d11") &&
            backend != QStringLiteral("vulkan") &&
            backend != QStringLiteral("opengl") &&
            backend != QStringLiteral("software"))
        {
            QTextStream(stderr)
                << "Invalid value for --rhi: " << backend
                << " (expected d3d11|vulkan|opengl|software)" << Qt::endl;
            return false;
        }
        qputenv("QSG_RHI_BACKEND", backend.toUtf8());
        return true;
    };

#if defined(Q_OS_WIN) || defined(Q_OS_ANDROID)
    const bool hasExplicitRhiEnv =
        QProcessEnvironment::systemEnvironment().contains(
            QStringLiteral("QSG_RHI_BACKEND"));
    if (!requestedRhi.isEmpty())
    {
        if (!applyRhiBackend(requestedRhi))
            return 2;
    }
    else if (!hasExplicitRhiEnv)
    {
#ifdef Q_OS_ANDROID
        qputenv("QT_QUICK_BACKEND", QByteArrayLiteral("software"));
        qputenv("QMLSCENE_DEVICE", QByteArrayLiteral("softwarecontext"));
        qputenv("QSG_RHI_BACKEND", QByteArrayLiteral("software"));
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        requestedRhi = QStringLiteral("software");
#else
        qputenv("QSG_RHI_BACKEND", QByteArrayLiteral("opengl"));
        requestedRhi = QStringLiteral("opengl");
#endif
    }
    else
    {
        requestedRhi = QString::fromLocal8Bit(qgetenv("QSG_RHI_BACKEND"))
                           .trimmed()
                           .toLower();
#ifdef Q_OS_ANDROID
        if (requestedRhi == QStringLiteral("software"))
        {
            qputenv("QT_QUICK_BACKEND", QByteArrayLiteral("software"));
            qputenv("QMLSCENE_DEVICE", QByteArrayLiteral("softwarecontext"));
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        }
#endif
    }
#else
    if (!requestedRhi.isEmpty() && !applyRhiBackend(requestedRhi))
        return 2;
#endif

    if (DebugTrace::verboseEnabled())
        DebugTrace::verbose(
            "app", QStringLiteral("startup args=%1 maxRenderPageSize=%2 rhi=%3")
                       .arg(rawArgs.join(' '))
                       .arg(maxRenderPageSize)
                       .arg(requestedRhi.isEmpty() ? QStringLiteral("(default)")
                                                   : requestedRhi));

#ifdef Q_OS_ANDROID
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(2, 0);
    format.setProfile(QSurfaceFormat::NoProfile);
    QSurfaceFormat::setDefaultFormat(format);
#endif

    QCoreApplication::setOrganizationName("StarryAgent");
    QCoreApplication::setApplicationName("StarryAgent");
#ifdef Q_OS_ANDROID
    QGuiApplication app(argc, argv);
#else
    QApplication app(argc, argv);
#endif

#ifdef Q_OS_ANDROID
    qputenv("QT_OPENSSL_LIBS", QByteArrayLiteral("libssl.so:libcrypto.so"));
    qputenv("QT_OPENSSL_PREFIX", QByteArrayLiteral("/data/data/moe.starry2233.StarryAgent/lib/x86_64"));
#endif

    DebugTrace::setupAutoExit(&app, 60000);

    // Basic Controls style so our warm palette isn't overridden by
    // Material/Fusion.
    QQuickStyle::setStyle("Basic");
    DebugTrace::verbose("app", QStringLiteral("QQuickStyle=Basic"));

    // Register bundled OFL fonts (display serif / body sans / mono / CJK
    // pairing) On Android, fonts ship as raw assets (NDK clang OOM on C++
    // embed).
#ifdef Q_OS_ANDROID
    const QStringList fontPaths = {
        QStringLiteral("assets:/fonts/Fraunces.ttf"),
        QStringLiteral("assets:/fonts/HankenGrotesk.ttf"),
        QStringLiteral("assets:/fonts/IBMPlexMono-Regular.ttf"),
        QStringLiteral("assets:/fonts/IBMPlexMono-Medium.ttf"),
        QStringLiteral("assets:/fonts/NotoSansSC.ttf"),
        QStringLiteral("assets:/fonts/NotoSerifSC.ttf"),
        QStringLiteral("assets:/fonts/SegoeUISymbol.ttf"),
    };
#else
    const QStringList fontPaths = {
        QStringLiteral(":/fonts/Fraunces.ttf"),
        QStringLiteral(":/fonts/HankenGrotesk.ttf"),
        QStringLiteral(":/fonts/IBMPlexMono-Regular.ttf"),
        QStringLiteral(":/fonts/IBMPlexMono-Medium.ttf"),
        QStringLiteral(":/fonts/NotoSansSC.ttf"),
        QStringLiteral(":/fonts/NotoSerifSC.ttf"),
        QStringLiteral(":/fonts/SegoeUISymbol.ttf"),
    };
#endif
    for (const QString &p : fontPaths)
    {
        if (QFontDatabase::addApplicationFont(p) < 0)
        {
            qWarning("Failed to load bundled font: %s", qPrintable(p));
        }
        else
        {
            DebugTrace::verbose("fonts", QStringLiteral("loaded %1").arg(p));
        }
    }
    // Segoe UI Symbol carries the star/sparkle glyphs (U+2726 etc.) that the
    // Latin display/body/mono faces lack. Register it as an explicit fallback
    // so Qt's font matcher finds it even where auto-merge is weak (Android).
    QFont::insertSubstitution("Fraunces", "Segoe UI Symbol");
    QFont::insertSubstitution("Hanken Grotesk", "Segoe UI Symbol");
    QFont::insertSubstitution("IBM Plex Mono", "Segoe UI Symbol");

    Config config;
    Settings settings(&config);
    if (!config.firstLaunch())
        settings.load();
    ThemeManager themeManager(&config, &settings);
    DebugTrace::verbose("config", QStringLiteral("rootDir=%1 firstLaunch=%2")
                                      .arg(config.rootDir())
                                      .arg(config.firstLaunch()));
    ToastService::bindSettings(&settings);
    AutoStartManager autoStartManager(&settings);

    // Headless bootstrap: create the .starryagent tree at the default root and
    // write default settings, then exit. Useful for scripting/CI/smoke tests.
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--setup")
    {
        if (config.firstLaunch())
            config.setRoot(config.defaultRoot());
        settings.save();
        return 0;
    }

    // Offline pipeline verification: feed a mock SSE byte stream through the
    // SseParser → StreamAssembler → ToolCallRecognizer chain and assert the
    // dispatch rule (no dispatch on partial arguments). No API key needed.
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--test-pipeline")
    {
        return runPipelineSmokeTest() ? 0 : 1;
    }

    // Offline tool verification: ensure the registry loads tools.jsonc, schemas
    // serialize, and edit/overwrite/exec run end-to-end. No API key needed.
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--test-tools")
    {
        return runToolsSmokeTest() ? 0 : 1;
    }

    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == "--test-web-search")
    {
        ToolRegistry toolRegistry(&config, &settings);
        QString captured = QStringLiteral("Error: timeout");
        QEventLoop loop;
        QObject::connect(&toolRegistry, &ToolRegistry::toolFinished, &app,
                         [&](const QString &ownerId, const QString &,
                             const QString &, const QString &result)
                         {
                             if (ownerId == QStringLiteral("smoke"))
                             {
                                 captured = result;
                                 loop.quit();
                             }
                         });
        toolRegistry.dispatch(
            QStringLiteral("smoke"), QStringLiteral("websearch1"),
            QStringLiteral("web_search"),
            QJsonObject{{QStringLiteral("query"), QStringLiteral("OpenAI")},
                        {QStringLiteral("limit"), 3}});
        QTimer::singleShot(45000, &loop, &QEventLoop::quit);
        loop.exec();

        QTextStream ts(stdout);
        ts << captured << Qt::endl;
        return captured.startsWith(
                   QStringLiteral("Web search results for query:"),
                   Qt::CaseInsensitive)
                   ? 0
                   : 1;
    }

    const bool demoMode =
        argc > 1 && QString::fromLocal8Bit(argv[1]) == "--demo";
    DebugTrace::verbose("app", QStringLiteral("demoMode=%1 logType=%2")
                                   .arg(demoMode)
                                   .arg(DebugTrace::logType()));

    ToolRegistry toolRegistry(&config, &settings);
    // Each Conversation owns its own OpenAIClient (created lazily on first
    // send), so multiple conversations can stream in parallel. The manager
    // injects the shared Settings + ToolRegistry into each conversation.
    ConversationManager conversations(&config, &settings, &toolRegistry,
                                      !demoMode);
    ScheduledTaskManager scheduledTasks(&config, &settings, &conversations);
    conversations.setScheduledTaskManager(&scheduledTasks);
    toolRegistry.setScheduledTaskManager(&scheduledTasks);
    ClipboardProxy clipboard;
#ifndef Q_OS_ANDROID
    TrayController trayController(&settings);
#endif
#ifndef Q_OS_ANDROID
    DesktopSelectionWindow desktopSelectionWindow;
#endif
    CameraBridge cameraBridge;
    FilePicker filePicker;
    ImageTransferService imageTransfer;
    CodeHighlighter codeHighlighter;
    ToastProxy toast;
#ifndef Q_OS_ANDROID
    QObject::connect(
        &scheduledTasks, &ScheduledTaskManager::taskNotification,
        &trayController,
        [&trayController](const QString &title, const QString &message)
        { trayController.showSystemNotification(title, message); });
#endif

    // Dev/demo mode: --demo injects fake turns into the active conversation
    // (content delta, tool call) so the ChatView + ToolCallCard state machine
    // can be exercised without an API key. The tool dispatch is REAL —
    // toolRegistry.dispatch runs overwrite and writes a real file.
    if (demoMode)
        conversations.newConversation(QStringLiteral("agent"));
    if (demoMode)
    {
        QTimer::singleShot(400, &app,
                           [&]
                           {
                               auto *c = conversations.active();
                               if (c)
                                   c->appendAssistantDelta(QStringLiteral(
                                       "I'll create a test file for you."));
                           });
        QTimer::singleShot(700, &app,
                           [&]
                           {
                               auto *c = conversations.active();
                               if (c)
                                   c->appendToolCall(
                                       QStringLiteral("demo1"),
                                       QStringLiteral("overwrite"));
                           });
        QTimer::singleShot(
            1100, &app,
            [&]
            {
                auto *c = conversations.active();
                if (!c)
                    return;
                QJsonObject args;
                args.insert("path", "demo_file.txt");
                args.insert("content", "StarryAgent demo wrote this.");
                const QString argsStr = QString::fromUtf8(
                    QJsonDocument(args).toJson(QJsonDocument::Indented));
                // setToolReady + dispatch directly (no live API call in demo
                // mode).
                c->setToolReady(QStringLiteral("demo1"),
                                QStringLiteral("overwrite"), argsStr, true);
                c->dispatch(QStringLiteral("demo1"),
                            QStringLiteral("overwrite"), argsStr);
            });
    }

    QQmlApplicationEngine engine;
#ifdef Q_OS_ANDROID
    engine.addImportPath(QStringLiteral("assets:/qt-project.org/imports"));
#endif
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings)
                     {
                         for (const QQmlError &warning : warnings)
                             qWarning().noquote()
                                 << QStringLiteral("[qml-warning] %1")
                                        .arg(warning.toString());
                     });
    MarkdownParser markdownParser;
    engine.rootContext()->setContextProperty("config", &config);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("themeManager", &themeManager);
    engine.rootContext()->setContextProperty("toolRegistry", &toolRegistry);
    engine.rootContext()->setContextProperty("conversations", &conversations);
    engine.rootContext()->setContextProperty("scheduledTasks", &scheduledTasks);
    engine.rootContext()->setContextProperty("markdownParser", &markdownParser);
    engine.rootContext()->setContextProperty("clipboard", &clipboard);
#ifndef Q_OS_ANDROID
    engine.rootContext()->setContextProperty("desktopSelectionWindow",
                                             &desktopSelectionWindow);
#endif
    engine.rootContext()->setContextProperty("cameraBridge", &cameraBridge);
    engine.rootContext()->setContextProperty("filePicker", &filePicker);
    engine.rootContext()->setContextProperty("imageTransfer", &imageTransfer);
    engine.rootContext()->setContextProperty("codeHighlighter",
                                             &codeHighlighter);
    engine.rootContext()->setContextProperty("toast", &toast);
    engine.rootContext()->setContextProperty("demoMode", demoMode);
    engine.rootContext()->setContextProperty("verboseLogging",
                                             DebugTrace::verboseEnabled());
    engine.rootContext()->setContextProperty("maxRenderPageSize",
                                             maxRenderPageSize);
    DebugTrace::verbose("qml", QStringLiteral("loading qrc:/ui/qml/main.qml"));
    engine.load(QUrl(QStringLiteral("qrc:/ui/qml/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;
    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst()))
    {
#ifndef Q_OS_ANDROID
        trayController.attach(&app, window);
#endif
    }

#ifdef Q_OS_WIN
    applyThemeToTopLevelWindows(engine.rootObjects(),
                                settings.theme() == QStringLiteral("dark"));
    QObject::connect(&settings, &Settings::themeChanged, &app,
                     [&engine, &settings]
                     {
                         applyThemeToTopLevelWindows(
                             engine.rootObjects(),
                             settings.theme() == QStringLiteral("dark"));
                     });
#endif

    return app.exec();
}
