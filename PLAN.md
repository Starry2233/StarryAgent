# 任务单
创建一个**跨平台**（安卓、MacOS、Linux 没有iOS）的AI Agent: StarryAgent
技术栈：Qt（要好看！）C:\Qt
要美观 类似豆包的设计
你搜一下 豆包风格UI（先搜索），简洁，不像AI写的，没有劣质动画和AI蓝紫配色
顾名思义，支持tool-calling（OpenAI格式和DeepSeek的格式都要兼容）
内置overwrite edit exec shell_exec（移动端需要Shizuku授权或者在有Root的情况下su 2000 -c, Shizuku SDK自己搜索 桌面端等价于exec） root_exec（移动端需要root Windows会用管理员身份运行 MacOS、Linux会让你输入密码）web_search web_fetch web_download sqlite3 broadcast（仅限安卓，其他平台返回Unavailable）
第一次开启时询问.starryagent目录存放位置，有三个选择：/sdcard/ /sdcard/Android/data/<包名>/data/ /data/data/<包名>/data/
目录下存放index.md tools.jsonc以及workspace skills memories文件夹
tools.jsonc
[
    {
        "id": "__built_in",
        "enabled": true
    },
    {
        "id": "xxx",
        "custom": true,
        "type": "mcp"或"cli"
        "config": { 
            // 你自己定 反正mcp和cli都要
        },
        "enabled": true
    }
]

index.md追加到原有的系统提示词中

memories为记忆系统
默认不预加载到prompt，AI需要显式调用recall_memory读取，调用write_memory写入
memory支持scope，默认conversation，也可以global共享给其他会话

skills参照OpenClaw
workspace是AI默认位置

AI的tool-calling需要用户点同意，除非在设置开启Bypass Permissions

还有多个对话历史 并且要处理并行问题

启动新对话时 让你选择Agent Mode Coding Mode Pal Mode三选一 第一个就是类似OpenClaw的普通Agent 第二个是类似Claude Code的代码编程助手 第三个更注重聊天，仅仅初始提示词不一样

OpenClaw: E:/OpenClaw
Claude Code（借鉴他的tool实现）: E:/doge-code

还有compact 获取这个模型的最大上下文然后达到了就总结压缩 默认开启 这个借鉴doge-code
