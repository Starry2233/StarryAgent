-- StarryAgent build config
-- xmake + clang-cl (MSVC ABI) + Qt 6.8.3 Quick + xrepo deps

set_project("StarryAgent")
set_languages("c++20")
-- set_arch("x64")
-- Prefer ccache when it is available. xmake falls back to the configured
-- compiler toolchain when no compatible ccache executable is found.
set_policy("build.ccache", true)

add_rules("mode.debug", "mode.release")

-- Windows: clang-cl toolchain (user requirement). xmake auto-loads MSVC sdk for headers/libs.
if is_plat("windows") then
    set_toolchains("clang-cl")
end

-- Qt Quick application rule (qrc compile + Qt frameworks + windows deploy)
-- xmake auto-detects the Qt SDK (C:/Qt/6.8.3/msvc2022_64).
add_rules("qt.quickapp")

-- third-party deps via xrepo (libcurl uses Windows Schannel for TLS; no openssl/Perl needed)
-- On Android, use Qt's QNetworkAccessManager for HTTP (no xrepo packages to avoid openssl build)
if not is_plat("android") then
    add_requires("nlohmann_json", "libcurl", "sqlite3", "libarchive")
else
    add_requires("nlohmann_json", "libarchive")
    -- Android NDK doesn't ship sqlite3 as system library; bundle via xrepo
    add_requires("sqlite3")
end

target("starryagent")
    set_kind("binary")
    add_files("src/main.cpp")
    add_files("src/core/Config.cpp", "src/core/Settings.cpp", "src/core/ProcessMemoryLimiter.cpp", "src/core/DebugTrace.cpp", "src/core/AutoStartManager.cpp")
    add_files("src/core/Config.h", "src/core/Settings.h", "src/core/ProcessMemoryLimiter.h", "src/core/DebugTrace.h", "src/core/AutoStartManager.h")   -- moc (Q_OBJECT)
    add_files("src/theme/ThemeMetadata.cpp", "src/theme/ThemeLoader.cpp", "src/theme/ThemeManager.cpp")
    add_files("src/theme/ThemeManager.h") -- moc (Q_OBJECT)
    add_files("src/api/SseParser.cpp", "src/api/StreamAssembler.cpp",
              "src/api/ToolCallRecognizer.cpp", "src/api/OpenAIClient.cpp",
              "src/api/pipeline_smoke.cpp")
    add_files("src/api/OpenAIClient.h")                      -- moc (Q_OBJECT)
    add_files("src/tools/ToolRegistry.cpp", "src/tools/EditTool.cpp",
              "src/tools/OverwriteTool.cpp", "src/tools/ExecTool.cpp",
              "src/tools/AndroidShellBridge.cpp",
              "src/tools/ShellExecTool.cpp", "src/tools/WebFetchTool.cpp",
              "src/tools/WebSearchTool.cpp", "src/tools/WebDownloadTool.cpp",
              "src/tools/Sqlite3Tool.cpp", "src/tools/RootExecTool.cpp",
              "src/tools/BroadcastTool.cpp", "src/tools/CliCustomTool.cpp",
              "src/tools/McpTool.cpp", "src/tools/MemoryToolUtils.cpp",
              "src/tools/RecallMemoryTool.cpp", "src/tools/WriteMemoryTool.cpp",
              "src/tools/ScheduledTaskTools.cpp",
              "src/tools/tools_smoke.cpp")
    add_files("src/tools/ToolRegistry.h")                   -- moc (Q_OBJECT)
    add_files("src/chat/Conversation.cpp", "src/chat/ConversationManager.cpp", "src/chat/ScheduledTaskManager.cpp", "src/chat/CompactSupport.cpp")
    add_files("src/chat/Conversation.h", "src/chat/ConversationManager.h", "src/chat/ScheduledTaskManager.h")  -- moc (Q_OBJECT)
    add_files("src/ui/MarkdownParser.cpp")
    add_files("src/ui/MarkdownParser.h")                    -- moc (Q_OBJECT)
    add_files("src/ui/ClipboardProxy.cpp", "src/ui/ClipboardProxy.h") -- moc (Q_OBJECT)
    add_files("src/ui/DesktopSelectionWindow.cpp", "src/ui/DesktopSelectionWindow.h") -- moc (Q_OBJECT)
    add_files("src/ui/FilePicker.cpp", "src/ui/FilePicker.h") -- moc (Q_OBJECT)
    add_files("src/ui/CameraBridge.cpp", "src/ui/CameraBridge.h") -- moc (Q_OBJECT)
    add_files("src/ui/ImageTransferService.cpp", "src/ui/ImageTransferService.h") -- moc (Q_OBJECT)
    add_files("src/ui/CodeHighlighter.cpp", "src/ui/CodeHighlighter.h") -- moc (Q_OBJECT)
    add_files("src/ui/ToastProxy.cpp", "src/ui/ToastProxy.h") -- moc (Q_OBJECT)
    add_files("src/ui/ToastService.cpp", "src/ui/ToastService.h") -- moc (Q_OBJECT)
    add_files("src/ui/TrayController.cpp", "src/ui/TrayController.h") -- moc (Q_OBJECT)
    if not is_plat("android") then
        add_files("external/qt-toast/msgtoast.cpp")
        add_files("external/qt-toast/msgtoast.h") -- moc (Q_OBJECT)
    end
    add_files("src/ui/ksyntaxhighlighting_logging.cpp")
    add_files("external/syntax-highlighting/src/lib/abstracthighlighter.cpp",
              "external/syntax-highlighting/src/lib/context.cpp",
              "external/syntax-highlighting/src/lib/contextswitch.cpp",
              "external/syntax-highlighting/src/lib/definition.cpp",
              "external/syntax-highlighting/src/lib/foldingregion.cpp",
              "external/syntax-highlighting/src/lib/format.cpp",
              "external/syntax-highlighting/src/lib/highlightingdata.cpp",
              "external/syntax-highlighting/src/lib/keywordlist.cpp",
              "external/syntax-highlighting/src/lib/repository.cpp",
              "external/syntax-highlighting/src/lib/rule.cpp",
              "external/syntax-highlighting/src/lib/state.cpp",
              "external/syntax-highlighting/src/lib/syntaxhighlighter.cpp",
              "external/syntax-highlighting/src/lib/theme.cpp",
              "external/syntax-highlighting/src/lib/themedata.cpp",
              "external/syntax-highlighting/src/lib/themepreviewicon.cpp",
              "external/syntax-highlighting/src/lib/wildcardmatcher.cpp",
              "external/syntax-highlighting/src/lib/worddelimiters.cpp")
    add_files("external/syntax-highlighting/data/themes/theme-data.qrc")
    add_files("src/qml.qrc")
    -- fonts.qrc omitted on Android (NDK clang OOM on large generated C++).
    -- Fonts are compiled as binary .rcc via on_load rule below.
    if not is_plat("android") then
        add_files("src/fonts.qrc")
    end
    add_includedirs("src", "src/ui", "src/theme", "external/syntax-highlighting/src/lib", "external/qt-toast")
    add_frameworks("QtQuick", "QtQml", "QtQuickControls2", "QtGui", "QtCore", "QtNetwork", "QtMultimedia")
    if not is_plat("android") then
        add_frameworks("QtWidgets")
    end
    if not is_plat("android") then
        add_packages("nlohmann_json", "libcurl", "sqlite3", "libarchive")
    else
        add_packages("nlohmann_json", "sqlite3", "libarchive")
    end

-- On Android, fonts are loaded directly from assets (no rcc to avoid OOM)
-- Sync all .ttf from src/ui/fonts/ to the Android build's assets/fonts/ after build.
if is_plat("android") then
    after_build(function (target)
        local proj = os.projectdir()
        local src = path.join(proj, "src/ui/fonts")
        local dst = path.join(proj, "build/.qt/app/android/starryagent/android-build/assets/fonts")
        if os.isdir(dst) then
            os.cp(path.join(src, "*.ttf"), dst)
        end

        local android_build = path.join(proj, "build/.qt/app/android/starryagent/android-build")
        if not os.isdir(android_build) then
            return
        end

        local java_root = path.join(android_build, "java", "org", "qtproject", "example", "starryagent", "shizuku")
        local aidl_root = path.join(android_build, "src", "org", "qtproject", "example", "starryagent", "shizuku")
        os.rm(path.join(android_build, "src", "org"))
        os.rm(path.join(android_build, "src", "qtproject"))
        os.rm(path.join(android_build, "src", "example"))
        os.rm(path.join(android_build, "src", "starryagent"))
        os.rm(path.join(android_build, "src", "shizuku"))
        os.rm(path.join(android_build, "src", "ShizukuRunner.java"))
        os.rm(path.join(android_build, "src", "StarryShellService.java"))
        os.rm(path.join(android_build, "src", "IStarryShellService.aidl"))
        os.rm(path.join(android_build, "java", "org"))
        os.rm(path.join(android_build, "aidl", "org"))
        os.rm(path.join(android_build, "src", "org"))

        local java_src = path.join(proj, "android", "java")
        local aidl_src = path.join(proj, "android", "aidl")
        if os.isdir(java_src) then
            os.mkdir(java_root)
            os.cp(path.join(java_src, "org", "qtproject", "example", "starryagent", "shizuku", "*.java"), java_root)
        end
        if os.isdir(aidl_src) then
            os.mkdir(aidl_root)
            os.cp(path.join(aidl_src, "org", "qtproject", "example", "starryagent", "shizuku", "*.aidl"), aidl_root)
        end

        local gradle_file = path.join(android_build, "build.gradle")
        if os.isfile(gradle_file) then
            local gradle = io.readfile(gradle_file)
            if gradle and not gradle:find("dev%.rikka%.shizuku:api:13%.1%.5", 1, false) then
                gradle = gradle:gsub("implementation 'androidx.core:core:1.13.1'",
                    "implementation 'androidx.core:core:1.13.1'\n    implementation 'androidx.annotation:annotation:1.8.2'\n    implementation 'dev.rikka.shizuku:api:13.1.5'\n    implementation 'dev.rikka.shizuku:provider:13.1.5'")
            end
            if gradle and not gradle:find("buildFeatures%s*%{", 1, false) then
                gradle = gradle:gsub("android%s*%{",
                    "android {\n    buildFeatures {\n        aidl true\n        buildConfig true\n    }", 1)
            end
            io.writefile(gradle_file, gradle)
        end

        local manifest_file = path.join(android_build, "AndroidManifest.xml")
        if os.isfile(manifest_file) then
            local manifest = io.readfile(manifest_file)
            if manifest and not manifest:find("rikka%.shizuku%.ShizukuProvider", 1, false) then
                local provider = [[

        <provider
            android:name="rikka.shizuku.ShizukuProvider"
            android:authorities="${applicationId}.shizuku"
            android:enabled="true"
            android:exported="true"
            android:multiprocess="false"
            android:permission="android.permission.INTERACT_ACROSS_USERS_FULL" />]]
                manifest = manifest:gsub("</application>", provider .. "\n    </application>", 1)
                io.writefile(manifest_file, manifest)
            end
        end
    end)
end

after_build(function (target)
    local data_src = path.join(os.projectdir(), "external", "syntax-highlighting", "data")
    if os.isdir(data_src) then
        local data_dst = path.join(target:targetdir(), "syntax-highlighting-data")
        os.rm(data_dst)
        os.cp(data_src, data_dst)
    end
end)
