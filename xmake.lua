-- StarryAgent build config
-- xmake + Qt 6.8.3 Quick + xrepo deps

set_project("StarryAgent")

local android_openssl_root = os.getenv("STARRY_ANDROID_OPENSSL_ROOT") or
                              os.getenv("ANDROID_OPENSSL_ROOT") or
                              os.getenv("OPENSSL_ANDROID_ROOT")

local libarchive_sources = {
    "external/libarchive/libarchive/archive_acl.c",
    "external/libarchive/libarchive/archive_blake2sp_ref.c",
    "external/libarchive/libarchive/archive_blake2s_ref.c",
    "external/libarchive/libarchive/archive_check_magic.c",
    "external/libarchive/libarchive/archive_cmdline.c",
    "external/libarchive/libarchive/archive_cryptor.c",
    "external/libarchive/libarchive/archive_digest.c",
    "external/libarchive/libarchive/archive_entry.c",
    "external/libarchive/libarchive/archive_entry_copy_bhfi.c",
    "external/libarchive/libarchive/archive_entry_copy_stat.c",
    "external/libarchive/libarchive/archive_entry_link_resolver.c",
    "external/libarchive/libarchive/archive_entry_sparse.c",
    "external/libarchive/libarchive/archive_entry_stat.c",
    "external/libarchive/libarchive/archive_entry_strmode.c",
    "external/libarchive/libarchive/archive_entry_xattr.c",
    "external/libarchive/libarchive/archive_hmac.c",
    "external/libarchive/libarchive/archive_match.c",
    "external/libarchive/libarchive/archive_options.c",
    "external/libarchive/libarchive/archive_pack_dev.c",
    "external/libarchive/libarchive/archive_parse_date.c",
    "external/libarchive/libarchive/archive_pathmatch.c",
    "external/libarchive/libarchive/archive_ppmd7.c",
    "external/libarchive/libarchive/archive_ppmd8.c",
    "external/libarchive/libarchive/archive_random.c",
    "external/libarchive/libarchive/archive_rb.c",
    "external/libarchive/libarchive/archive_read.c",
    "external/libarchive/libarchive/archive_read_add_passphrase.c",
    "external/libarchive/libarchive/archive_read_append_filter.c",
    "external/libarchive/libarchive/archive_read_data_into_fd.c",
    "external/libarchive/libarchive/archive_read_disk_entry_from_file.c",
    "external/libarchive/libarchive/archive_read_disk_posix.c",
    "external/libarchive/libarchive/archive_read_extract.c",
    "external/libarchive/libarchive/archive_read_extract2.c",
    "external/libarchive/libarchive/archive_read_open_fd.c",
    "external/libarchive/libarchive/archive_read_open_file.c",
    "external/libarchive/libarchive/archive_read_open_filename.c",
    "external/libarchive/libarchive/archive_read_open_memory.c",
    "external/libarchive/libarchive/archive_read_set_format.c",
    "external/libarchive/libarchive/archive_read_set_options.c",
    "external/libarchive/libarchive/archive_read_support_filter_all.c",
    "external/libarchive/libarchive/archive_read_support_filter_by_code.c",
    "external/libarchive/libarchive/archive_read_support_filter_bzip2.c",
    "external/libarchive/libarchive/archive_read_support_filter_compress.c",
    "external/libarchive/libarchive/archive_read_support_filter_grzip.c",
    "external/libarchive/libarchive/archive_read_support_filter_gzip.c",
    "external/libarchive/libarchive/archive_read_support_filter_lrzip.c",
    "external/libarchive/libarchive/archive_read_support_filter_lz4.c",
    "external/libarchive/libarchive/archive_read_support_filter_lzop.c",
    "external/libarchive/libarchive/archive_read_support_filter_none.c",
    "external/libarchive/libarchive/archive_read_support_filter_program.c",
    "external/libarchive/libarchive/archive_read_support_filter_rpm.c",
    "external/libarchive/libarchive/archive_read_support_filter_uu.c",
    "external/libarchive/libarchive/archive_read_support_filter_xz.c",
    "external/libarchive/libarchive/archive_read_support_filter_zstd.c",
    "external/libarchive/libarchive/archive_read_support_format_7zip.c",
    "external/libarchive/libarchive/archive_read_support_format_all.c",
    "external/libarchive/libarchive/archive_read_support_format_ar.c",
    "external/libarchive/libarchive/archive_read_support_format_by_code.c",
    "external/libarchive/libarchive/archive_read_support_format_cab.c",
    "external/libarchive/libarchive/archive_read_support_format_cpio.c",
    "external/libarchive/libarchive/archive_read_support_format_empty.c",
    "external/libarchive/libarchive/archive_read_support_format_iso9660.c",
    "external/libarchive/libarchive/archive_read_support_format_lha.c",
    "external/libarchive/libarchive/archive_read_support_format_mtree.c",
    "external/libarchive/libarchive/archive_read_support_format_rar.c",
    "external/libarchive/libarchive/archive_read_support_format_rar5.c",
    "external/libarchive/libarchive/archive_read_support_format_raw.c",
    "external/libarchive/libarchive/archive_read_support_format_tar.c",
    "external/libarchive/libarchive/archive_read_support_format_warc.c",
    "external/libarchive/libarchive/archive_read_support_format_xar.c",
    "external/libarchive/libarchive/archive_read_support_format_zip.c",
    "external/libarchive/libarchive/archive_string.c",
    "external/libarchive/libarchive/archive_string_sprintf.c",
    "external/libarchive/libarchive/archive_time.c",
    "external/libarchive/libarchive/archive_util.c",
    "external/libarchive/libarchive/archive_version_details.c",
    "external/libarchive/libarchive/archive_virtual.c",
}
if is_plat("windows") then
    table.insert(libarchive_sources, "external/libarchive/libarchive/archive_windows.c")
    table.insert(libarchive_sources, "external/libarchive/libarchive/archive_read_disk_windows.c")
    table.insert(libarchive_sources, "external/libarchive/libarchive/filter_fork_windows.c")
else
    table.insert(libarchive_sources, "external/libarchive/libarchive/archive_disk_acl_linux.c")
    table.insert(libarchive_sources, "external/libarchive/libarchive/filter_fork_posix.c")
end
set_languages("c++20")
-- set_arch("x64")
-- Prefer ccache when it is available. xmake falls back to the configured
-- compiler toolchain when no compatible ccache executable is found.
set_policy("build.ccache", true)

add_rules("mode.debug", "mode.release")

-- Windows defaults to clang-cl (MSVC ABI).
-- Android keeps xmake's native Android/NDK toolchain selection.
-- Other non-Windows targets default to LLVM/Clang unless the user passes a
-- toolchain explicitly.
if not get_config("toolchain") then
    if is_plat("windows") then
        set_toolchains("clang-cl")
    elseif not is_plat("android") then
        set_toolchains("llvm")
    end
end

-- third-party deps via xrepo
-- Windows keeps libcurl for desktop and uses Schannel for TLS there.
-- Android packaging stages prebuilt OpenSSL runtime libs separately, so
-- avoid pulling xrepo openssl3 into the Android native dependency graph.
add_requires("nlohmann_json", "sqlite3", "zlib", "zstd", "bzip2")
if not is_plat("android") then
    add_requires("libcurl")
end

target("libarchive_vendor")
    set_kind("static")
    set_languages("c99")
    add_files(libarchive_sources, {sourcekind = "cc"})
    add_headerfiles("external/libarchive/libarchive/*.h")
    add_includedirs("external/libarchive/libarchive", "external/libarchive/build/xmake", {public = true})
    add_defines("LIBARCHIVE_STATIC", {public = true})
    if is_plat("android") then
        add_includedirs("external/libarchive/contrib/android/include")
        if android_openssl_root and #android_openssl_root > 0 then
            local android_openssl_abi_root = path.join(android_openssl_root, get_config("arch") or "")
            add_includedirs(path.join(android_openssl_abi_root, "include"))
            add_linkdirs(path.join(android_openssl_abi_root, "lib"))
        end
    end
    add_defines("PLATFORM_CONFIG_H=\"config.h\"", "LIBARCHIVE_STATIC", "__LIBARCHIVE_BUILD")
    if is_plat("windows") then
        add_defines("_CRT_SECURE_NO_WARNINGS", "_WIN32_WINNT=0x0601")
    elseif is_plat("android") then
        add_defines("_GNU_SOURCE")
    else
        add_defines("_GNU_SOURCE")
    end
    if is_plat("android") then
        add_cflags("-fPIC", {force = true})
    end
    add_packages("zlib", "zstd", "bzip2", {public = true})

target("starryagent")
    if is_plat("android") then
        add_rules("qt.shared")
        set_kind("shared")
        add_cxflags("-fPIC", {force = true})
        on_config(function (target)
            target:set("rpathdirs", {})
            target:add("ldflags", "-Wl,--disable-new-dtags", "-Wl,--rpath=''", {force = true})
            target:add("shflags", "-Wl,--disable-new-dtags", "-Wl,--rpath=''", {force = true})
        end)
    else
        add_rules("qt.quickapp")
        set_kind("binary")
    end
    add_files("src/main.cpp")
    add_files("src/core/Config.cpp", "src/core/Settings.cpp", "src/core/ProcessMemoryLimiter.cpp", "src/core/DebugTrace.cpp", "src/core/AutoStartManager.cpp")
    add_files("src/core/Config.h", "src/core/Settings.h", "src/core/ProcessMemoryLimiter.h", "src/core/DebugTrace.h", "src/core/AutoStartManager.h")   -- moc (Q_OBJECT)
    add_files("src/theme/ThemeMetadata.cpp", "src/theme/ThemeManager.cpp", "src/theme/ThemeLoader.cpp")
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
    add_files("src/ui/AndroidBackgroundRuntime.cpp", "src/ui/AndroidBackgroundRuntime.h") -- moc (Q_OBJECT)
    add_files("src/ui/ImageTransferService.cpp", "src/ui/ImageTransferService.h") -- moc (Q_OBJECT)
    add_files("src/ui/CodeHighlighter.cpp", "src/ui/CodeHighlighter.h") -- moc (Q_OBJECT)
    add_files("src/ui/ToastProxy.cpp", "src/ui/ToastProxy.h") -- moc (Q_OBJECT)
    add_files("src/ui/ToastService.cpp", "src/ui/ToastService.h") -- moc (Q_OBJECT)
    if not is_plat("android") then
        add_files("src/ui/TrayController.cpp", "src/ui/TrayController.h") -- moc (Q_OBJECT)
    end
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
    add_packages("nlohmann_json", "sqlite3")
    if not is_plat("android") then
        add_packages("libcurl")
    end
    add_deps("libarchive_vendor")

    after_build(function (target)
        local data_src = path.join(os.projectdir(), "external", "syntax-highlighting", "data")
        if os.isdir(data_src) then
            local data_dst = path.join(target:targetdir(), "syntax-highlighting-data")
            os.rm(data_dst)
            os.cp(data_src, data_dst)
        end
    end)
