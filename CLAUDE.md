# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project status

Greenfield. The only source of truth right now is `PLAN.md` — the task spec. There is no build system, no source tree, and no tests yet. Treat `PLAN.md` as authoritative for product behavior; this file captures the architecture decisions a future Claude instance needs to be productive.

## What StarryAgent is

A cross-platform AI Agent desktop/mobile app built with **Qt 6.8.3 (C:/Qt)**, targeting **Android, macOS, Linux** (no iOS). UI must be Doubao-style: clean, non-AI-looking, no cheap animations, no AI blue/purple palette. The product is a conversational agent with tool-calling, permission gating, multiple parallel conversations, and a compact (context-compression) feature.

## UI design philosophy

The Doubao-style constraint means the *spirit* of Doubao (clean, friendly, rounded, spacious) executed in a distinctive palette — a literal Doubao clone in blue would itself violate the no-AI-blue/purple rule. Commit to one bold direction up front: **warm editorial minimalism** — warm paper/cream surfaces, near-black ink, a single confident accent (terracotta/clay), a refined star/sparkle mark playing on the "Starry" name. Assistant prose should read like typeset text, not a rounded gradient bubble.

- **Typography** — no Inter/Roboto/Arial/system defaults. Pair a characterful display face with a refined body face; bundle font files as Qt resources (`:/fonts/...`), register via `QFontDatabase::addApplicationFont`, apply on QML `Text`/`TextArea`. CJK localization needs a matching CJK serif+sans pairing (e.g., Source Han / Noto CJK), not fallback rendering.
- **Color & theme** — one dominant tone + sharp accents, not a timid even palette. Define every color as a token in a single theme object (`Theme.qml` singleton or a C++ `Theme`), never hardcoded per component. Explicitly reject: purple-on-white gradients, the AI blue/violet default, generic Material purple.
- **Motion** — purposeful only. One orchestrated entrance (staggered reveals) beats scattered micro-interactions. In QML use `Transition` + `NumberAnimation`/`SpringAnimation` with easing (`OutCubic`/`OutQuint`); avoid bouncy springs unless intentional; respect a reduced-motion preference. No idle loops, no cheap bounces.
- **Spatial composition** — asymmetry, overlap, controlled density or generous negative space; break cookie-cutter card grids. The mode picker (Agent/Coding/Pal) is the hero moment — make it the one thing users remember.
- **Atmosphere & depth** — no flat solids. Subtle grain, layered surfaces, soft shadows, decorative hairlines. In QML: `MultiEffect`/`DropShadow`, layered `Rectangle`s with subtle gradients, a `ShaderEffect` for paper grain. Keep it restrained and warm — paper-like, not glassy/AI.
- **Reject on sight** — Inter/Roboto, purple-on-white gradients, identical card grids, scattered decorative animations, generic chatbot bubbles.

### Layout & controls (Doubao reference)

Verified against Doubao's web bundle (`chat.75aadec8.css`, fetched 2026-06-30). Borrow the *measured structure*; swap the palette to warm.

- **Token theming**: Doubao drives all color from a `--dbx-*` custom-property set, toggled by a `data-theme`/`data-theme-mode` attribute on the root, with light and dark sharing token names. Mirror this in QML: a single `Theme` object (or `Theme.qml` singleton) holding every color/radius/spacing token, with light + dark sets switched by one property. Do not hardcode colors in components.
- **Surfaces layer neutrals, not pure white**: light uses `#f4f4f4` app bg / `#fff` surfaces / `#fff` popovers; dark uses `#1a1a1a` / `#212121` / `#363636`. StarryAgent warms these (cream/paper light, tinted-black dark) — same layered-depth structure, warm hue.
- **Sidebar**: 280px on desktop, collapses to 0 below a 600px viewport. Primary "新对话" at top; conversation list grouped by recency (今天 / 昨天 / 7天内 / 更早); text-only rows with a quiet hover fill; user/settings pinned at the bottom.
- **Chat column**: centered, `min(available-width, center-content-max-width)`; single vertical message flow. Assistant text sits on the background (no bubble) with the star/sparkle avatar to the left; user messages are a soft right-aligned bubble. Rich markdown, code blocks with a copy affordance and a dedicated syntax-highlight palette, follow-up suggestion chips beneath assistant turns.
- **Input dock**: rounded textarea with attachment/voice/send affordances inside (icon target 28px); a model/mode selector sits just above it. Grows with content up to a cap, then scrolls internally.
- **Radius scale**: 8px small controls, 12px cards, 16px larger surfaces, 9999px (pill) for chips and primary buttons.
- **Shadows & motion**: low-opacity soft shadows like `0 6px 10px rgba(42,60,79,.1)`; transitions `.2–.4s` on `cubic-bezier(.4,0,.1,1)`. No bounces.
- **Breakpoints** (for responsive QML mobile↔desktop): 462 / 640 / 868 / 1440px.
- **Deliberate divergences**: Doubao's brand is `#0066ff` (blue) and it ships system fonts (SF Pro / Segoe UI / PingFang SC / MiSans). StarryAgent replaces blue with terracotta/clay and *bundles* characterful display+body fonts (per the no-generic-fonts rule), including a CJK pairing for localization.

## Reference projects — borrow, don't copy

Two external repos are designated as the source of patterns. They live outside this working directory and are read-only references:

- **`E:/doge-code`** — a Claude Code fork (TypeScript/Bun). Borrow from it:
  - Tool implementation pattern — see `src/tools/` (each tool is a self-contained dir, e.g. `FileEditTool/`, `BashTool/`, `WebFetchTool/`). StarryAgent reuses the *shape*: a central tool registry (`src/tools.ts`) + per-tool modules, adapted to C++/Qt.
  - Compact feature — see `src/services/compact/compact.ts` + `src/commands/compact/`. StarryAgent's compact fetches the model's max context, and when a conversation reaches it, summarizes/compresses prior turns. **On by default.**
  - Slash-command registry shape — `src/commands.ts` + `src/commands/`.
- **`E:/OpenClaw`** — an agent platform (TypeScript/Node). Borrow from it:
  - **Skills** system — see `extensions/open-prose/skills/prose/SKILL.md` for the frontmatter format (`name`, `description`, `metadata`) and activation model. StarryAgent's `skills/` folder uses this skill-pack convention.
  - Agent Mode behavior (the "Agent Mode" below is modeled on OpenClaw's general agent).

When in doubt about how a feature should behave, read the corresponding file in the reference repo first. Do **not** port TypeScript verbatim — translate to Qt/C++ idioms.

## Tech stack & cross-platform rules

- Qt 6.8.3 at `C:/Qt`. Build via CMake (Qt's modern default). A `CMakeLists.txt` at repo root is expected once scaffolding begins.
- Platform targets: Android (primary mobile), macOS, Linux. **No iOS.**
- Mobile-only Android primitives that must degrade gracefully on desktop:
  - `broadcast` — Android-only; returns "Unavailable" on macOS/Linux/Windows.
  - `root_exec` — Android needs root; macOS/Linux prompt for password; Windows runs as admin.
  - `exec` on Android requires Shizuku authorization, or `su 2000 -c` when rooted. Integrate the Shizuku SDK for the Shizuku path. On desktop, `exec` == `root_exec` equivalent.
- First launch asks the user where to put the `.starryagent` directory. Three choices (Android): `/sdcard/`, `/sdcard/Android/data/<pkg>/data/`, `/data/data/<pkg>/data/`.

## `.starryagent` directory layout

The chosen root holds:

- `index.md` — appended to the base system prompt.
- `tools.jsonc` — tool registry config. Built-in entry is `{"id":"__built_in","enabled":true}`; custom entries are `type: "mcp" | "cli"` with a `config` object (define the schema for both mcp and cli when implementing) and `enabled` flag.
- `workspace/` — AI's default working directory.
- `skills/` — skill packs (OpenClaw convention).
- `memories/` — memory system.
  - Runtime behavior: memories are **not** auto-injected into prompts.
  - The model must call `recall_memory` to read them and `write_memory` to persist them.
  - `scope: "conversation" | "global"` is supported. Default is conversation.

## Built-in tools (must ship)

`overwrite`, `edit`, `exec`, `shell_exec`, `root_exec`, `web_search`, `web_fetch`, `web_download`, `sqlite3`, `broadcast` (Android-only, else "Unavailable"). Tool-calling uses the OpenAI `tool_calls` format; DeepSeek is OpenAI-compatible, so one adapter covers both (see below).

## Model API & streaming tool-calling

- **Adapter target: OpenAI Chat Completions** (`/v1/chat/completions`). One adapter covers OpenAI, DeepSeek, and any OpenAI-compatible endpoint (DeepSeek exposes an OpenAI-compatible API, which is how PLAN.md's "OpenAI + DeepSeek" requirement is satisfied). Base URL, API key, and model list are user-configurable.
- **Streaming (SSE)**: parse `data:` chunks, accumulate `delta.content` into the live assistant message, and accumulate `delta.tool_calls[].function.arguments` (which arrive as partial JSON-string fragments) into a per-tool-call buffer keyed by `tool_call.id`.
- **Tool-call dispatch rule (critical)**: dispatch a tool call for execution ONLY after its full `arguments` JSON has been received, parsed, and validated. The streaming recognizer must NEVER execute on partial arguments. Completion is signaled by `finish_reason == "tool_calls"` (or stream close). Until then, the in-progress tool call renders as a "composing" card, not an executable one.
- **Tool-call shape**: OpenAI `tool_calls` (`id`, `type:"function"`, `function:{name, arguments:<JSON string>}`). Map to the built-in tool registry; on result, return a `role:"tool"` message carrying `tool_call_id`.
- **Streaming toggle**: a "流式输出" setting (default on). Off → non-streaming request, the assistant message appears as one complete response. The tool-call dispatch rule above applies in both modes.

## Three agent modes

On new conversation, the user picks one — only the initial system prompt differs:

1. **Agent Mode** — general agent, OpenClaw-style.
2. **Coding Mode** — code assistant, Claude-Code-style (borrow doge-code's tool set).
3. **Pal Mode** — chat-focused.

## Permissions & concurrency

- Every tool call requires explicit user approval **unless** the user has enabled "Bypass Permissions" in settings.
- Multiple conversation histories must be supported, and **parallel** tool-call/concurrency issues must be handled (don't assume single-threaded turn-taking).

## Working in this repo

- When scaffolding starts, create `CMakeLists.txt`, a `src/` tree, and per-tool modules under `src/tools/` mirroring doge-code's per-tool-directory shape.
- Before finishing any UI work, verify it against the **UI design philosophy** section above.
- Don't invent build/lint/test commands until a build system exists. Once CMake is in place, `cmake -B build && cmake --build build` is the baseline.

## Android build notes

Successful Android debug APK build on 2026-07-02 (Windows verified path):

- The repo currently builds with `xmake`, not Gradle/CMake directly at the root.
- Use `C:/Qt/6.8.3` as the single Qt SDK baseline. For Android, use `C:/Qt/6.8.3/android_arm64_v8a` with host tools from `C:/Qt/6.8.3/mingw_64`.
- Use JDK 17 at `D:/Program Files/Java/jdk-17`.
- Use Android SDK at `C:/Users/Administrator/AppData/Local/Android/Sdk`.
- A working NDK is `C:/Users/Administrator/AppData/Local/Android/Sdk/ndk/27.2.12479018`.
- Qt's Android OpenSSL TLS backend does not bundle OpenSSL itself.
  `scripts/build_android_gradle.ps1` copies `libssl*.so` / `libcrypto*.so`
  from an existing xmake `openssl3` package cache or from
  `STARRY_ANDROID_OPENSSL_ROOT` / `-AndroidOpenSslRoot`. If those are missing,
  it can download a prebuilt `.tar.gz` from
  `STARRY_ANDROID_OPENSSL_ARCHIVE_URL` / `-AndroidOpenSslArchiveUrl`, optionally
  verify `STARRY_ANDROID_OPENSSL_ARCHIVE_SHA256` /
  `-AndroidOpenSslArchiveSha256`, and cache it under
  `build/android/openssl-cache` or `-AndroidOpenSslCacheDir`. Do not add
  `openssl3` as a main Android target dependency on Windows; OpenSSL's final
  link can exceed the Windows argv length limit. Without those runtime
  libraries, networking falls back to `cert-only` and logs
  `Active TLS backend does not support key creation` /
  `QSslSocket::connectToHostEncrypted: TLS initialization failed`.

### Recommended two-stage command

Use the repository script for normal Windows builds. It deliberately builds the
native library with xmake and packages it with the Gradle wrapper as separate
stages; it does not ask xmake/androiddeployqt to generate the APK.

```powershell
# Known-good native configuration: arm64-v8a, Qt 6.8.3, NDK API 35, release .so.
$env:STARRY_ANDROID_OPENSSL_ARCHIVE_URL='https://example.com/android-openssl-prebuilt.tar.gz'
$env:STARRY_ANDROID_OPENSSL_ARCHIVE_SHA256='<optional sha256>'
pwsh -File scripts/build_android_gradle.ps1 -Phase native

# Package the staged Gradle project as a debug APK.
pwsh -File scripts/build_android_gradle.ps1 -Phase package -GradleVariant debug
```

The default ABI is intentionally `arm64-v8a`. Build WSA's native compatibility
target separately when required; do not put it in the default package:

```powershell
pwsh -File scripts/build_android_gradle.ps1 -Abis x86_64 -Phase all
```

The script validates the staged `libstarryagent.so` has no ELF `RPATH`/
`RUNPATH`, then verifies the final APK contains exactly the requested ABI(s),
the ABI-suffixed app library, `libc++_shared.so`, Qt Core, and the Android Qt
platform plugin. If Qt's OpenSSL TLS backend is present, the script also
verifies that `libssl*.so` and `libcrypto*.so` are packaged into the same ABI
directory.

Configure xmake for arm64-v8a:

```powershell
xmake f -c -p android -a arm64-v8a -m release --qt=C:\Qt\6.8.3\android_arm64_v8a --qt_host=C:\Qt\6.8.3\mingw_64 --android_sdk=C:\Users\Administrator\AppData\Local\Android\Sdk --ndk=C:\Users\Administrator\AppData\Local\Android\Sdk\ndk\27.2.12479018 --ndk_sdkver=35 --build_toolver=35.0.0 --runtimes=c++_shared -y
```

Build native code:

```powershell
$env:JAVA_HOME='D:\Program Files\Java\jdk-17'
$env:Path="$env:JAVA_HOME\bin;$env:Path"
xmake -vD -y
```

If `xmake` reaches `generating.qt.app starryagent.apk` and then hangs or reports `unknown architecture`, the native library may already be built successfully at:

```text
build/android/arm64-v8a/release/libstarryagent.so
```

In that case, stop the stuck `xmake`/`androiddeployqt` processes and package through the generated Gradle project:

```powershell
cd build\.qt\app\android\starryagent\android-build
$env:JAVA_HOME='D:\Program Files\Java\jdk-17'
$env:Path="$env:JAVA_HOME\bin;$env:Path"
# Windows
.\gradlew.bat assembleDebug --no-daemon --stacktrace
.\gradlew.bat --stop
# macOS/Linux with pwsh
./gradlew assembleDebug --no-daemon --stacktrace
./gradlew --stop
```

The successful APK output path was:

```text
build/.qt/app/android/starryagent/android-build/build/outputs/apk/debug/android-build-debug.apk
```

Observed warning: Android Gradle Plugin 8.6.0 warns that `compileSdk = 37` is newer than its tested range. This did not block APK generation.

## Android Shizuku notes

- As of 2026-07-06, Android `exec` / `shell_exec` now have a real Shizuku path instead of being TODO-only.
- Implementation layout:
  - C++ bridge: `src/tools/AndroidShellBridge.cpp`
  - Java bridge: `android/java/moe/starry2233/StarryAgent/shizuku/ShizukuRunner.java`
  - Shizuku user service: `android/java/moe/starry2233/StarryAgent/shizuku/StarryShellService.java`
  - AIDL: `android/aidl/moe/starry2233/StarryAgent/shizuku/IStarryShellService.aidl`
- Android packaging must inject:
  - Gradle deps: `dev.rikka.shizuku:api:13.1.5`, `dev.rikka.shizuku:provider:13.1.5`, `androidx.annotation:annotation:1.8.2`
  - `buildFeatures { aidl true; buildConfig true }`
  - manifest provider:
    ```xml
    <provider
        android:name="rikka.shizuku.ShizukuProvider"
        android:authorities="${applicationId}.shizuku"
        android:enabled="true"
        android:exported="true"
        android:multiprocess="false"
        android:permission="android.permission.INTERACT_ACROSS_USERS_FULL" />
    ```
- Current runtime behavior:
  - Android `shell_exec` only accepts `sh`.
  - If Shizuku is running but permission is missing, the app requests permission and returns an error asking the user to retry.
  - If Shizuku is unavailable, the native fallback tries `su 2000 -c`.
- Verified build path:
  - `xmake` still tends to fail at the final APK generation step with `llvm-readobj` noise, but native build succeeds.
  - Manual Gradle packaging succeeded after syncing the Java/AIDL files:
    `build/.qt/app/android/starryagent/android-build/build/outputs/apk/debug/android-build-debug.apk`
