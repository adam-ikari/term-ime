---
id: website-spread-redesign
title: "网站改版以利传播：中文社区优先、双钩子首屏、结构重排"
category: decision
status: active
tags: [website, 传播, 文案]
created: "2026-09-23T04:02:42"
updated: "2026-09-24T10:35:05"
---

<!-- compiled_truth -->
**决定**：官网落地页按“可传播”重做，范围 = 结构 + 文案 + 视觉全做；主战场是中文技术社区（V2EX/掘金/知乎/公众号）。

**首屏标题（2026-09-24 修订）**：Hero 主标题用**事实陈述**，不用痛点式钩子。当前为「终端里的中文输入法」，副标题承担具体卖点（不装 X/Wayland/D-Bus，静态单文件，SSH/Docker/WSL/无桌面 server）。原「终端上，终于能打中文了」判为标题党，已在 hero H1、`og:title`、`twitter:title`、meta description、`README.md` 首行、`static/llms.txt` 统一替换。标题仍须含品牌词 `term-ime`（否则 Google 会改写成 term-time）。

**信息架构（自上而下）**：Hero + 一条 curl 安装命令（带复制）+ 动画终端演示 → 场景徽章条 → 三条痛点（为什么需要）→ 三种做法对比表 → 快速开始 → 特性卡（收益导向）→ 可嵌入库（term-ime-lib / term-terminal）→ FAQ（与 JSON-LD 一致）→ 分享 CTA（复制链接 / Star）。

**理由**：旧版是“功能清单 + 6 条场景”，读者 3 秒内抓不到痛点，也没有任何可复制转发的元素（无安装命令、无对比、无分享入口）。中文社区的转发动力来自痛点共鸣与一句话可复述的结论，硬核数字负责建立可信度——但可信度靠**可验证的事实**，不靠情绪化钩子。

**硬约束（文案不得编造）**：单文件完全静态（本机构建 5.3 MB，`ldd` 显示 not a dynamic executable）；预编译仅 linux x86_64，其他架构需源码编译；一键安装 `curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash`（默认 ~/.local，免 sudo）；librime 拼音 + 5 组独立模糊音开关；零 X / Wayland / D-Bus 依赖；MIT。

**波及面**：`website/src/pages/index.tsx` + `index.module.css`、`docusaurus.config.ts`（tagline/og/twitter 标题）、`static/img/og-image.png`（重做）、`static/llms.txt`、`README.md` 首行、`website/docs/intro.mdx` 首段。


## Timeline

- time: 2026-09-23T04:02:42
  kind: decision
  summary: "Created this page: 网站改版以利传播：中文社区优先、双钩子首屏、结构重排"
  source: "用户指令：重新设计当前网站和文案以利于传播"
  affects: [website-spread-redesign]

- time: 2026-09-23T04:03:02
  kind: decision
  summary: "确定传播向改版的三个方向与信息架构"
  source: "用户在本次会话中的三项选择"
  affects: [website-spread-redesign]

- time: 2026-09-24T10:34:52
  kind: reversal
  summary: "首屏钩子标题「终端上，终于能打中文了」被判定为标题党，替换为事实陈述「终端里的中文输入法」；og/twitter/meta description/README/llms.txt 同步去钩子"
  source: "本次会话用户指令：替换 hero/site 标题为非标题党文案"
  affects: [website-spread-redesign]

- time: 2026-09-24T10:35:05
  kind: decision
  summary: "首屏钩子标题改为事实陈述"
  source: "本次会话用户指令"
  affects: [website-spread-redesign]
