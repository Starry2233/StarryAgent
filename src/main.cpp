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
#include <QAbstractNativeEventFilter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
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
#include "core/LanguageManager.h"
#include "core/ProcessMemoryLimiter.h"
#include "core/Settings.h"
#include "skills/SkillInstallManager.h"
#include "tools/ToolRegistry.h"
#include "theme/ThemeManager.h"
#include "tools/tools_smoke.h"
#include "ui/AndroidBackgroundRuntime.h"
#include "ui/AndroidPermissionBridge.h"
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
#include "ui/AppWindowChrome.h"
#include "ui/StarryWindow.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#ifdef Q_OS_ANDROID
#include <signal.h> // 后面要考
#endif

#include <cstdio>
#include <iostream>

#ifdef QT_STATIC
#include <QtPlugin>
Q_IMPORT_PLUGIN(QtQuick2Plugin)
Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
Q_IMPORT_PLUGIN(QtQuickControls2BasicStylePlugin)
Q_IMPORT_PLUGIN(QtQuickControls2BasicStyleImplPlugin)
Q_IMPORT_PLUGIN(QtQuickControls2ImplPlugin)
Q_IMPORT_PLUGIN(QtQuickEffectsPlugin)
Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
Q_IMPORT_PLUGIN(QtQuick_WindowPlugin)
Q_IMPORT_PLUGIN(QMultimediaQuickModule)
Q_IMPORT_PLUGIN(QWindowsMediaPlugin)
Q_IMPORT_PLUGIN(QtQmlPlugin)
Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
Q_IMPORT_PLUGIN(QtQmlWorkerScriptPlugin)
#endif

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

void applyWindowsChromeToTopLevelWindows(const QList<QObject *> &objects,
                                        bool dark)
{
    for (QObject *object : objects)
    {
        if (auto *window = qobject_cast<QWindow *>(object))
            AppWindowChrome::applyToWindow(window, dark);
    }
}

class WindowsChromeFilter : public QAbstractNativeEventFilter
{
  public:
    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override
    {
        if (eventType != QStringLiteral("windows_generic_MSG"))
            return false;

        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_NCCREATE)
        {
            CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(msg->lParam);
            if (cs)
                cs->dwExStyle |= WS_EX_DLGMODALFRAME;
        }
        return false;
    }
};
} // namespace
#endif

#ifdef Q_OS_ANDROID
namespace
{
/*        -----------------------------------------------
                                                  #
                ###         ##########           ###
                ###        ############          ###
                ###        ###########   #############
                ###             #        #############
            #   ###   #        ###        ############
           ###  ###  ###  #############       ######
           ###  ###  ###  #############       ## ###
           ###  ###  ###   ############      ### ###
            #   ###   #       #####         ###  ###
     ##         ###           #####       #####  ###
   ###         ####          ##  ###     ####  #####
   ####        ####         ##    ####   ###   #####
      ##        ##         ###     ####          ##      是sb
       ##                 ###       ###
        ###              ###
          ###         ####
           ##############
              ########
*/
// Okii (Imoo) is the worst company I've ever seen, they enjoy the CLOSE-SOURCE Linux kernel source every day
// and they removed libre apps from their app store replacing them with their own closed-source apps,
// so Okii, fuck you, and I hope your company goes bankrupt. StarryAgent will not run on Okii devices for security reasons.
// By the way, if you are using Okii's device, please use other devices for armeabi-v7a architecture, thank you.

// 对中国用户：
// 小天才是我见过的最糟糕的公司，他们每天都在享受闭源的 Linux 内核源码，并且他们从应用商店中移除了自由应用，
// 用他们自己的闭源应用替换了它们，所以小天才，去你妈的，我希望你们公司破产。StarryAgent 不会在小天才设备上运行，出于安全原因。
// 顺便说一句，如果您正在使用 Okii 的设备，请在 armeabi-v7a 架构上使用其他设备，谢谢。
// 还有如果你在小天才设备上root并且被云控封了30天及以上，欢迎加入小天才讨论邮件列表 xtc-community@googlegroups.com
// GNU四大自由：
// 自由使用：任何人都可以出于任何目的使用软件。
// 自由研究：任何人都可以研究软件的工作原理，并根据自己的需要进行修改。
// 自由分发：任何人都可以自由地分发软件的副本。
// 自由改进：任何人都可以改进软件，并将改进后的版本发布给公众，以便整个社区受益。
// 小天才他妈的天天享受别人的劳动成果，却一点回报都不愿意给
// 新机型是越来越他妈贵的
// 他们的闭源应用是越来越他妈多的
// 他们的闭源应用是越来越他妈恶心的
// 他们的闭源应用是越来越他妈垃圾的
// 他们的闭源应用是越来越他妈无耻的
// 他们的系统是越来越他妈sb的
// 我就想问问：一个连用户都不尊重的公司，凭什么让用户尊重他？
// 让我们一起抵制小天才，抵制垃圾软件，抵制垃圾公司，抵制垃圾系统，抵制垃圾应用，抵制垃圾硬件，
// 抵制垃圾设备，抵制垃圾厂商，抵制垃圾企业，抵制垃圾公司
// 不是你们这番b操作让社区才有的破解、bot、卖号、主页刷赞吗
// 我们改你们的应用怎么啦，改你们系统怎么啦，这不是我们的自由吗
// 社区公约里面禁止这个东西，肯定会遭到报应的
// 如果你现在是通过Github看到的，请记住，社区公约只是公约（骗你的 是shit），要敢于打破

bool isRunningInImooDevice()
{
    // Check if persist.xtc.alxcse in getprop output
    FILE *fp = popen("getprop", "r");
    if (!fp)
        return false;
    // Read the output line by line and check for "persist.xtc.alxcse"
    char buffer[256];
    bool found = false;
    while (fgets(buffer, sizeof(buffer), fp))
    {
        if (strstr(buffer, "persist.xtc.alxcse") != nullptr)
        {
            found = true;
            break;
        }
    }
    pclose(fp);
    // Then check if /xtcdata exists
    // If persist.xtc.alxcse is not found, check if /xtcdata exists
    if (!found)
    {
        QFile xtcDataFile(QStringLiteral("/xtcdata"));
        found = xtcDataFile.exists();
        xtcDataFile.close();
    }
    return found;
}
}
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    attachWindowsParentConsole();
#endif
#ifdef Q_OS_ANDROID
    /*
     * Because Imoo devices monitor application data and there are risks of uploading and data, 
     * for security reasons, if the StarryAgent detects that it is running on an Imoo device, 
     * it will send a SIGSEGV signal to itself and crash. 
     * Please use devices other than Imoo for armeabi-v7a architecture.
    */
    if (isRunningInImooDevice())
    {
// for libre
#ifdef FUCK_IMOO
        // Then send SIGSEGV signal to self
        raise(SIGSEGV);
        return 1;
#endif
    }
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
        qputenv("QSG_RHI_BACKEND", QByteArrayLiteral("opengl"));
        requestedRhi = QStringLiteral("opengl");
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
    app.setQuitOnLastWindowClosed(false);
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
    LanguageManager languageManager(&settings);
    languageManager.initialize();
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
    SkillInstallManager skillInstallManager(&config, &settings, toolRegistry.skillManager());
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
    AndroidBackgroundRuntime androidBackgroundRuntime;
    CameraBridge cameraBridge;
    AndroidPermissionBridge androidPermissionBridge(&config);
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

    qmlRegisterType<StarryWindow>("StarryAgent", 1, 0, "StarryWindow");

#ifdef Q_OS_WIN
    WindowsChromeFilter chromeFilter;
    app.installNativeEventFilter(&chromeFilter);
#endif

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
    engine.rootContext()->setContextProperty("languageManager", &languageManager);
    engine.rootContext()->setContextProperty("themeManager", &themeManager);
    engine.rootContext()->setContextProperty("skillInstallManager", &skillInstallManager);
    engine.rootContext()->setContextProperty("toolRegistry", &toolRegistry);
    engine.rootContext()->setContextProperty("conversations", &conversations);
    engine.rootContext()->setContextProperty("scheduledTasks", &scheduledTasks);
    engine.rootContext()->setContextProperty("markdownParser", &markdownParser);
    engine.rootContext()->setContextProperty("clipboard", &clipboard);
#ifndef Q_OS_ANDROID
    engine.rootContext()->setContextProperty("desktopSelectionWindow",
                                             &desktopSelectionWindow);
#endif
    engine.rootContext()->setContextProperty("androidBackgroundRuntime",
                                             &androidBackgroundRuntime);
    engine.rootContext()->setContextProperty("cameraBridge", &cameraBridge);
    engine.rootContext()->setContextProperty("androidPermissionBridge",
                                             &androidPermissionBridge);
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
    languageManager.bindEngine(&engine);
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
    applyWindowsChromeToTopLevelWindows(
        engine.rootObjects(), settings.theme() == QStringLiteral("dark"));
    QObject::connect(&settings, &Settings::themeChanged, &app,
                     [&engine, &settings]
                     {
                         applyWindowsChromeToTopLevelWindows(
                             engine.rootObjects(),
                             settings.theme() == QStringLiteral("dark"));
                     });
#endif

    return app.exec();
}
