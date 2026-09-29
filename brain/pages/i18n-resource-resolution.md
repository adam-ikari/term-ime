---
id: i18n-resource-resolution
title: "i18n 翻译资源解析：可执行文件相对 share/term-ime/translations 优先，安装链路必须打包"
category: decision
status: active
tags: [i18n, translation, install, packaging]
created: "2026-09-24T10:34:27"
updated: "2026-09-29T02:07:04"
---

<!-- compiled_truth -->
**结论**：翻译资源按固定顺序解析，第一个含 `zh-CN.json` 的目录胜出：

1. `<cwd>/data/translations`（开发：build 目录直跑）
2. `<exe_dir>/../share/term-ime/translations`、`<exe_dir>/data/translations`（便携/前缀安装；用 `/proc/self/exe` 解析，静态二进制的 `argv[0]` 可能只是 PATH 里的裸名字，不可靠）
3. `$HOME/.local/share/term-ime/translations`（install.sh 默认前缀）
4. `/usr/share/term-ime/translations`（系统安装）

**约束（打包）**：任何发布产物（release tarball、CI 的 install-test tarball、本地 pack 脚本）**必须**把 `data/translations/*.json` 放到 `share/term-ime/translations/`。缺了它二进制照样启动，只是设置面板回落成未翻译（或缺键）文案——静默劣化，不会报错。

**波及面**：`.github/workflows/release.yml`、`.github/workflows/ci.yml`（install-test 打包 + `Installed translations` 断言 + `translations/zh-CN.json` 存在断言）、`CMakeLists.txt` 的 `install(DIRECTORY data/translations ...)`、`website/static/install.sh`（已有逻辑）。


## Timeline

- time: 2026-09-24T10:34:27
  kind: decision
  summary: "Created this page: i18n 翻译资源解析：可执行文件相对 share/term-ime/translations 优先，安装链路必须打包"
  source: "本次会话：安装后设置面板文案缺失（I18nFix）"
  affects: [i18n-resource-resolution]

- time: 2026-09-24T10:34:39
  kind: decision
  summary: "记录解析顺序与打包约束"
  source: "本次会话 I18nFix"
  affects: [i18n-resource-resolution]

- time: 2026-09-28T14:19:48
  kind: decision
  summary: "翻译解析两处定调：(1) load_translations 先装内置表、再用文件条目逐条覆盖（合并语义），这样安装目录下过旧的 en.json/zh-CN.json 不会让设置面板打印裸 key；文件为空或全非字符串则视为失败回退内置。(2) 资源搜索顺序把 CWD 的 data/translations 排到最后（exe 相对 → ~/.local/share → /usr/local/share → /usr/share → CWD），避免不可信当前目录压过已安装文件。"
  source: "2026-09-28 代码评审修复轮"
  affects: [i18n-resource-resolution]

- time: 2026-09-29T02:07:04
  kind: note
  summary: "rime 共享数据目录（rime-data）改用同一条规则解析：exe 相对优先、以内容标记判定归属，见 rime-data-dir-discovery。i18n 与 rime-data 现在是同一个安装形状（share/term-ime/...）的两份线索，打包缺任一份都是静默劣化。"
  source: "v1.1.4 修复会话"
  affects: [i18n-resource-resolution]
