---
id: website-spread-redesign
title: "网站改版以利传播：中文社区优先、双钩子首屏、结构重排"
category: decision
status: active
tags: [website, 传播, 文案]
created: "2026-09-23T04:02:42"
updated: "2026-09-28T02:15:17"
---

<!-- compiled_truth -->
**决定**：官网落地页按“可传播”重做，范围 = 结构 + 文案 + 视觉全做；主战场是中文技术社区（V2EX/掘金/知乎/公众号）。

**首屏标题（2026-09-28 修订）**：Hero 主标题用**事实陈述**，不用痛点式钩子。当前为「终端输入法」，站名标题为 `term-ime — 终端输入法`。演进：「终端上，终于能打中文了」（判为标题党）→「终端里的中文输入法」→「终端输入法」。统一替换面：hero H1、`docusaurus.config.ts` 的 `title` / `og:title` / `twitter:title` / meta description、首页 Layout `description`、`README.md` 首行、`static/llms.txt` 引言、`website/docs/intro.mdx` 首行；描述里的关键词短语「终端中文输入法」保持不变。站点级标题仍必须含品牌词 `term-ime`（否则 Google 会改写成 term-time）。分享卡标题同规则：`og-image.html` 现为「SSH 上，直接打中文」，旧钩子「SSH 上，终于能打中文了」已废弃。

**文案风格（2026-09-28 定）**：社区推广口吻，保留痛点共鸣和转发钩子，但标题一律写成 README 式直白短语（`没有输入法的终端有多难用` / `装完怎么用` / `功能` / `常见问题`），不用口号式小标题、排比三连、「不是 X 而是 Y」、段尾总结句；数字和硬事实照旧（0 图形栈依赖、1 个静态二进制、5 组模糊音、MIT）。首页 FAQ 与 JSON-LD 用同一份 `FAQS` 数组，页面显示什么搜索引擎就看到什么。

**Hero tagline 只给两个信息（2026-09-28 复盘）**：一句痛点场景 + 一个卖点，禁止把卖点排成三连短句（反例已被打回：「直接读写终端字符流，零图形依赖，一条命令装完」= 排比三连 + 四字口号 + 短句收尾，全是 AI 腔）。当前版本：「SSH 上改配置、写 commit，写句中文得回桌面，打完再粘回来。term-ime 把输入法装进终端，没有桌面的机器也能用。」其余事实交给数字条和安装框去说。

**禁用句式「不装 X、不装 Wayland、不碰 D-Bus」（2026-09-28 用户判定 AI 腔）**：连排否定加技术名词罗列是模型常用写法，全站禁用，同类的「不需要 A、B 或 C」也照此改。同一个事实改用正面说法，按位置各选一种、别反复用：`零图形依赖`、`直接读写终端字符流`、`桌面环境和图形输入法框架都用不上`，或直接列场景（SSH / 容器 / 无桌面服务器）。已替换：hero tagline、`docusaurus.config.ts` 的 tagline 与 meta/og/twitter description、JSON-LD description、首页 Layout description、`README.md` 第 3 行、`static/llms.txt` 引言、`docs/intro.mdx` 开头两处、`og-image.html` 副标题（原破折号连排一并去掉）。

**信息架构（2026-09-28 精简为 6 段）**：Hero（标题 + tagline + curl 安装命令 + 演示终端 + 4 个数字 + 「用在哪」场景徽章条）→ 痛点三卡 + 对比表（同段，小标题分隔）→ 装完怎么用（启动 / 切中文 / 打字上屏 三步）→ 功能六卡 + 嵌入库两卡（同段）→ FAQ → 分享 CTA。原来 8 段合并为 6 段；按键全流程、配置字段、渲染细节等下沉到 `docs/quickstart`（新增「日常使用」）、`docs/shortcuts`、`docs/config`、`docs/tui-component`。

**理由**：旧版是“功能清单 + 6 条场景”，读者 3 秒内抓不到痛点，也没有任何可复制转发的元素（无安装命令、无对比、无分享入口）。中文社区的转发动力来自痛点共鸣和一句话可复述的结论，硬核数字负责建立可信度——但可信度靠**可验证的事实**，不靠情绪化钩子。

**硬约束（文案不得编造）**：单文件完全静态（本机构建 5.3 MB，`ldd` 显示 not a dynamic executable）；预编译仅 linux x86_64，其他架构需源码编译；一键安装 `curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash`（默认 ~/.local，免 sudo）；librime 拼音 + 5 组独立模糊音开关；零 X / Wayland / D-Bus 依赖（事实成立，但表述见上面的禁用句式）；MIT。

**改文案后的固定流程**：`npm run build` → `node scripts/font-subset.mjs check`（引入新字会缺字，须 `write --src /tmp/stsn` 重新子集化，否则落到 fallback 字体破坏字形统一）→ 改了 `og-image.html` 还要按 `website/README.md` 用 chromium + convert 重出 `static/img/og-image.png`。

**波及面**：`website/src/pages/index.tsx` + `index.module.css`（新增 `.chipsLabel`、`.subTitle`）、`docusaurus.config.ts`（title/tagline/og/twitter/description）、`og-image.html` + `static/img/og-image.png`（已按新文案重出）、`static/img/` 字体子集 `stsn-*.woff2` + `subsetchars.txt`（891 字）、`static/llms.txt`、`README.md` 首行与第 3 行、`website/docs/intro.mdx`、`website/docs/quickstart.md`。


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

- time: 2026-09-28T01:22:29
  kind: decision
  summary: "首屏标题与站名标题统一为「终端输入法」，同步 hero/og/twitter/meta/README/llms.txt/intro"
  source: "本次会话用户指令：网站 hero 和标题改为终端输入法"
  affects: [website-spread-redesign]

- time: 2026-09-28T01:22:29
  kind: decision
  summary: "全站标题统一为「终端输入法」：hero H1、site title（term-ime — 终端输入法）、og/twitter title、meta 与首页 description、README 首行、llms.txt 引言、intro.mdx 首行同步"
  source: "本次会话用户指令：网站 hero 和标题改为终端输入法（用户选定全站一致范围）"
  affects: [website-spread-redesign]

- time: 2026-09-28T01:45:48
  kind: decision
  summary: "文案改为社区推广口吻（去 AI 味），首页由 8 段精简为 6 段，细节下沉到文档"
  source: "本次会话用户指令：文案不要有 AI 味，重新设计（范围=文案+版式结构，语气=社区推广口吻）"
  affects: [website-spread-redesign]

- time: 2026-09-28T01:45:48
  kind: decision
  summary: "首页重排：痛点与对比表合并、特性与嵌入库合并（8 段→6 段）；标题改为 README 式直白短语，去掉口号、排比与段尾总结；新增 .chipsLabel/.subTitle 样式；FAQ 与 JSON-LD 改为共用 FAQS 数组；按键与配置细节下沉 docs/quickstart「日常使用」"
  source: "本次会话用户指令：文案不要有 AI 味 重新设计"
  affects: [website-spread-redesign]

- time: 2026-09-28T01:55:33
  kind: decision
  summary: "禁用「不装 X、不碰 D-Bus」连排否定句式；og-image 换成事实陈述标题"
  source: "本次会话用户指令：不碰XX就是AI常用的说法"
  affects: [website-spread-redesign]

- time: 2026-09-28T01:55:33
  kind: decision
  summary: "禁用连排否定句式「不装 X、不装 Wayland、不碰 D-Bus」（用户判定 AI 腔）：hero tagline、meta/og/twitter description、config tagline、JSON-LD、README 第 3 行、llms.txt、intro.mdx 全部改为「零图形依赖 / 直接读写终端字符流 / 都用不上 / 列场景」等正面说法；分享卡旧钩子「SSH 上，终于能打中文了」换成「SSH 上，直接打中文」，og-image.png 已重出；字体子集按新用字重生成（891 字）"
  source: "本次会话用户指令：不碰XX就是AI常用的说法；SSH 上，终于能打中文了 这个文案不好"
  affects: [website-spread-redesign]

- time: 2026-09-28T02:15:17
  kind: decision
  summary: "Hero tagline 收敛为痛点场景 + 单个卖点，禁三连排比卖点"
  source: "本次会话用户指令：这还是AI的说话方式（用户选定「没桌面也能用」版本）"
  affects: [website-spread-redesign]

- time: 2026-09-28T02:15:17
  kind: decision
  summary: "Hero tagline 二次返工：旧句「直接读写终端字符流，零图形依赖，一条命令装完」被判为 AI 腔（排比三连 + 四字口号 + 短句收尾），改为痛点场景 + 单个卖点：「…term-ime 把输入法装进终端，没有桌面的机器也能用。」"
  source: "本次会话用户指令：这还是AI的说话方式"
  affects: [website-spread-redesign]
