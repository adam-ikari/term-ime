---
id: i18n-resource-resolution
title: "i18n 翻译资源解析：可执行文件相对 share/term-ime/translations 优先，安装链路必须打包"
category: decision
status: active
tags: [i18n, translation, install, packaging]
created: "2026-09-24T10:34:27"
updated: "2026-09-24T10:34:39"
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
