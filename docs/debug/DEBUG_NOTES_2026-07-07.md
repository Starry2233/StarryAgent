# StarryAgent 调试记录（2026-07-07）

这份文档只记录目前已经观察到的现象、已经确认的异常点、以及最可能的原因。  
不包含“已经修好”的结论。

## 1. 主要问题

### 1.1 打开某些历史消息时，Windows 端会卡死，甚至出现：

- 应用内存暴涨
- 显存暴涨
- `dwm` 黑屏 / 重启 / 系统卡死几十秒
- 不是只在流式阶段卡，加载历史时也会卡

这说明问题不只是“流式输出重复渲染”，更像是：

1. 历史数据本身就带有高代价内容
2. 某个渲染路径在“加载已有消息”时也会触发重布局 / 重绘 / 大量纹理分配
3. 可能存在 QML / Qt Quick 的 GPU 纹理或 scene graph 放大问题

---

## 2. 已确认的历史记录异常

历史目录：

`C:\Users\Administrator\.starryagent\conversations`

### 2.1 某些会话里有很多“空 assistant 占位项”

例如：

- `69eef27b-4dbc-49a6-8254-eee774ec87fc.json`
- `9feb56ab-fd42-4d23-aec5-585ec985c253.json`

特征：

- `tool` 项很多
- `assistant` 空文本项很多

统计示例：

- `69eef27b-4dbc-49a6-8254-eee774ec87fc.json`
  - `Items=20`
  - `ToolCount=5`
  - `AssistantEmpty=5`
- `9feb56ab-fd42-4d23-aec5-585ec985c253.json`
  - `Items=22`
  - `ToolCount=10`
  - `AssistantEmpty=8`

可能原因：

1. 流式阶段先创建 assistant 占位项
2. tool-call 中间状态又追加项
3. 某些占位项最终没有被合并 / 覆盖 / 清理

后果：

- 历史列表里同一轮对话会拆成很多块
- QML delegate 数量增多
- 组件数、布局次数、绑定求值次数都增加

---

### 2.2 工具结果被完整保存，尤其是 `web_search`

例如 `69eef27b-4dbc-49a6-8254-eee774ec87fc.json` 中，`tool` 项的 `result` 保存了完整搜索结果列表：

- 多个链接
- 多行文本
- 完整 reminder

这类内容虽然文本量不算特别夸张，但会导致：

1. 历史记录比必要的大
2. 每次加载会多渲染很多不该面向用户直接显示的中间数据
3. 如果这些块也走 Markdown/Text 渲染，会额外增加格式解析和布局成本

更合理的设计通常是：

- 历史里保留 tool-call 元数据
- tool 原始结果只保留摘要，或仅在调试模式保留

---

### 2.3 有明确包含 Markdown 表格的历史

最典型的是：

- `b142ff28-7e76-4deb-9d2f-7bfb7cb8b82f.json`

统计：

- `Items=12`
- `MaxPipes=72`

其中包含多段表格文本，例如：

- 小表格
- 多列多行表格

以及：

- `69eef27b-4dbc-49a6-8254-eee774ec87fc.json`
 里也有 assistant 回复包含 Markdown 表格

```text
| 功能 | 说明 |
|------|------|
| 安装任意APK | 绕过官方应用商店限制 |
...
```

这和“打开历史就卡”的现象高度相关。

---

## 3. 目前最可疑的原因排序

### 原因 A：Markdown 表格渲染路径本身有高代价问题

这是当前最优先怀疑项。

理由：

1. 你之前已经明确反馈“markdown 表格渲染会卡一下”
2. 现在历史里确实存在表格内容
3. 历史加载也卡，说明不是只在流式时触发

可能的具体问题：

- `Text.MarkdownText` 对表格支持路径代价高
- QML `TextEdit` / `Text` 在大段 markdown 下反复布局
- 表格被拆分 / 重算多次
- 表格渲染触发异常大的 scene graph 纹理

---

### 原因 B：历史里 tool/assistant 占位项过多，导致组件数量膨胀

即使单条文本不大，delegate 数量一多，也会导致：

- `ListView` 创建过多项
- 每条消息内部的 `MarkdownView` / `TextEdit` / `CodeBlock` 重复初始化
- 列表滚动区域内容高度反复计算

这更像“放大器”，未必是根因，但会显著加重卡顿。

---

### 原因 C：图片 / Markdown / 富文本混排导致 Qt Quick 纹理或布局异常

尤其是包含：

- 图片
- 富文本
- 多段消息
- 长列表

一起加载时，Qt Quick 可能：

- 生成过大纹理
- 频繁回收 / 重建 scene graph 节点
- 在硬件渲染下引发 GPU 压力

这解释了为什么你观察到的是“显存爆”而不只是普通内存涨。

---

### 原因 D：历史恢复逻辑没有做“展示层压缩”

当前持久化结果更像“运行时 item 树原样存档”，而不是“适合重新展示的简化历史”。

问题在于：

- 中间 tool 状态
- 空 assistant 项
- 原始 tool 结果
- 多余过渡项

都被带回 UI 了。

这会让“加载历史”比“当时实时显示”更重。

---

## 4. 当前代码里值得重点看的位置

### 会话存储 / 加载

- [src/chat/Conversation.cpp](E:/StarryAgent/src/chat/Conversation.cpp:1)
- [src/chat/ConversationManager.cpp](E:/StarryAgent/src/chat/ConversationManager.cpp:1)

重点看：

- `items` 是怎么序列化的
- 空 assistant 项为什么会被保留
- tool 结果为什么全量入库
- 历史加载时是否做了合并 / 清理

---

### Markdown 渲染

- [src/ui/qml/components/MarkdownView.qml](E:/StarryAgent/src/ui/qml/components/MarkdownView.qml:1)
- [src/ui/MarkdownParser.cpp](E:/StarryAgent/src/ui/MarkdownParser.cpp:1)

重点看：

- `markdownParser.parse(text)` 的分段策略
- `TextEdit { textFormat: Text.MarkdownText }` 的代价
- 表格是否还在走原生 markdown 渲染
- streaming 时虽然对 trailing table 做了截断，但历史加载不会走这层保护

---

### 列表与消息 delegate

- [src/ui/qml/ChatView.qml](E:/StarryAgent/src/ui/qml/ChatView.qml:1)

重点看：

- 历史消息加载时，是否一次性创建太多 delegate
- 是否有 `implicitHeight` / `contentHeight` 链式重算
- 图片与 markdown 组合时是否有重复布局

---

## 5. 已观察到的高风险会话

### `b142ff28-7e76-4deb-9d2f-7bfb7cb8b82f.json`

- 标题：`你好`
- 特征：多段 Markdown 表格
- `MaxPipes=72`

这是最像“表格渲染问题复现样本”的会话。

---

### `69eef27b-4dbc-49a6-8254-eee774ec87fc.json`

- 标题：`我这个是什么手表`
- 特征：
  - 有图片
  - 有 `web_search` 工具结果
  - 有空 assistant 占位项
  - 有 assistant Markdown 表格
- `Items=20`

这是“混合富内容导致崩”的高风险样本。

---

### `9feb56ab-fd42-4d23-aec5-585ec985c253.json`

- 标题：`在d盘创建文件夹写一个rust 猜数字游戏`
- 特征：
  - `ToolCount=10`
  - `AssistantEmpty=8`

这更像“中间状态过多”的结构问题样本。

---

## 6. 更可能不是根因的项

### 不是单纯文件太大

当前最大会话文件只有 22 KB 左右。  
这不足以单独解释系统级卡死。

所以问题更像：

- “某种内容结构”触发了坏的渲染路径
- 而不是“数据量大到普通解析扛不住”

---

### 不太像只是流式重复渲染

因为你已经确认：

- 历史加载也会卡

所以即使流式阶段有重复渲染，也不是唯一原因。

---

## 7. 最值得优先验证的假设

### 假设 1

**只要加载带 Markdown 表格的历史，就会触发 Qt Quick 富文本 / Markdown 渲染异常。**

验证方法：

- 暂时把历史中的表格文本替换成纯文本占位
- 或直接禁用 `Text.MarkdownText`
- 再打开同一会话看是否还爆

---

### 假设 2

**不是表格本身，而是“表格 + 图片 + 多 tool 项 + 空 assistant 占位项”叠加导致 delegate 数量和布局链爆炸。**

验证方法：

- 拿 `69eef27b-...` 会话做裁剪版本
- 分别去掉：
  - 图片
  - tool 项
  - 空 assistant 项
  - 表格
- 逐步二分

---

### 假设 3

**历史恢复时没有合并临时项，导致 UI 恢复成本远高于实时会话。**

验证方法：

- 在 load 阶段临时丢弃：
  - 空 assistant 项
  - tool 的完整 `result`
  - 中间占位项
- 看是否显著改善

---

## 8. 建议的排查顺序

1. 先只拿 `b142ff28-...` 复现  
   最纯粹，集中验证表格渲染

2. 再拿 `69eef27b-...` 复现  
   验证“图片 + 表格 + tool 项”组合问题

3. 最后看 `9feb56ab-...`  
   验证结构膨胀问题

4. 如果你要上日志，优先打这些点：
   - 历史加载每条 item 创建时
   - `MarkdownParser::parse`
   - `MarkdownView.refreshSegments`
   - 每个消息 delegate `Component.onCompleted`
   - 表格块 / 代码块 / 图片块创建时
   - `ListView` 可见项数量变化

---

## 9. 一句话判断

当前最像的问题不是“聊天记录写坏了”，而是：

**历史记录把运行时中间状态、tool 原始结果、空 assistant 占位项都保存了下来，再叠加 Markdown 表格渲染路径本身的高代价，最终在加载历史时把 Qt Quick 渲染链压爆了。**
