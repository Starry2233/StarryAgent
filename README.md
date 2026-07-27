# StarryAgent

StarryAgent 是一个基于 Qt / QML 的跨平台 AI Agent 应用，面向 Android、macOS 和 Linux，目标是把对话、工具调用和本地执行能力整合进一个统一的桌面 / 移动端体验里。

## Highlights

- Qt 6.8.3 + QML 构建的实际应用骨架，而不只是规格文档
- 多模式对话入口：Agent / Coding / Pal
- OpenAI 兼容的流式 tool-calling 管线与 smoke test
- 内置工具注册系统、权限确认与并行会话能力
- 可安装主题系统，支持 `.tar.zst` 主题包、字体与壁纸
- Android 方向已包含 Shizuku 相关桥接与打包处理

## What is StarryAgent

StarryAgent 当前定位为一个跨平台 AI Agent 客户端：

- 使用 Qt / QML 构建统一 UI
- 对接 OpenAI 兼容模型接口
- 支持工具调用、本地执行、权限 gating
- 计划覆盖移动端与桌面端的长期使用场景

仓库里已经包含主源码、Android 桥接代码、主题系统、示例资源和当前构建脚本，因此它不再只是概念原型；但产品规格与长期目标仍以 `PLAN.md` 为准。

## Project Status

项目仍在持续开发中。

当前可以明确看到的落地内容包括：

- `src/` 下的实际应用源码
- `android/` 下的 Java / AIDL / Shizuku 桥接
- `xmake.lua` 驱动的当前构建入口
- `src/main.cpp` 中可见的 demo / smoke test 启动参数
- `docs/PROGRESS.md` 中记录的阶段性实现进展

同时也应注意：README 不替代产品规格文档，也不把长期规划写成“已完全完成”的功能列表。若要理解产品目标、实现约束与当前进度，优先看下面这些文档：

- `PLAN.md`：产品规格与目标
- `CLAUDE.md`：仓库协作与实现约束
- `docs/PROGRESS.md`：阶段性实现进展
- `docs/debug/DEBUG_NOTES_2026-07-07.md`：历史调试记录

## Quick Start

先初始化子模块：

```bash
git submodule update --init --recursive
```

当前仓库的主构建入口是 `xmake.lua`。桌面端最小构建流程：

```bash
xmake f -m debug -y
xmake
xmake run starryagent
```

仓库里还带有几个离线 smoke test 入口，可直接通过程序参数触发：

```bash
xmake run starryagent -- --test-pipeline
xmake run starryagent -- --test-tools
```

Android、Qt SDK、JDK、NDK 的已验证构建记录目前整理在 `CLAUDE.md` 中。

## Repository Layout

```text
.
├─ src/           主源码（API、聊天、工具、主题、UI）
├─ android/       Android 专属 Java / AIDL / 平台桥接
├─ assets/        静态资源
├─ examples/      示例主题与样例资源
├─ scripts/       辅助脚本
├─ external/      第三方依赖子模块
├─ third_party/   预留的第三方目录
├─ PLAN.md        产品规格
├─ CLAUDE.md      仓库工作约束
├─ docs/          进展与调试等补充文档
└─ xmake.lua      当前构建入口
```

### 目录说明

- `src/`
  - 主工程代码，包含 `api/`、`chat/`、`core/`、`modes/`、`theme/`、`tools/`、`ui/` 等模块。
- `android/`
  - Android 平台专用实现，包含 Shizuku 相关 Java / AIDL 桥接。
- `examples/`
  - 当前包含主题示例，如 `examples/themes/cute-clouds/` 与对应打包文件。
- `external/`
  - 仓库依赖的外部子模块，不应当按普通源码目录随意移动或忽略。
- `scripts/`
  - 本地辅助脚本与调试脚本。

## Build Notes

从当前构建配置可见，项目使用：

- C++20
- Qt 6.8.3
- xrepo 依赖管理
- 主要依赖：`nlohmann_json`、`libcurl`、`sqlite3`、`libarchive`

这些信息以 `xmake.lua` 为当前仓库状态的直接依据。

## Key Documents

- `PLAN.md`
  - 产品功能、平台范围、工具能力、交互模式等规格来源。
- `CLAUDE.md`
  - 面向本仓库协作时的实现约束、设计方向、Android 构建记录。
- `docs/PROGRESS.md`
  - 当前阶段的落地进度记录。
- `docs/debug/DEBUG_NOTES_2026-07-07.md`
  - 一份历史调试调查笔记，适合作为内部参考，不是正式产品文档。

## Example Assets

当前仓库已经包含主题示例资源，可作为主题系统的参考输入：

- `examples/themes/cute-clouds/theme.json`
- `examples/themes/cute-clouds.tar.zst`

## Submodules

当前 `external/` 下有两个重要子模块：

- `external/qt-toast`
- `external/syntax-highlighting`

不要把整个 `external/` 当作可随意整理或忽略的目录。

## License

本仓库当前附带 `LICENSE` 文件（GPLv3 文本）。

在对外发布或引入更多第三方依赖前，建议再次核对整体许可证策略。
