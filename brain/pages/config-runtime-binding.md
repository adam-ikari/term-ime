---
id: config-runtime-binding
title: "配置字段必须真正接线：shell 优先级、日志、rime 数据目录"
category: decision
status: active
tags: [config, shell, logging, rime]
created: "2026-09-28T14:20:29"
updated: "2026-09-28T19:35:44"
---

<!-- compiled_truth -->
配置字段只有被运行时读到才算接线。本页面记录哪些字段真的生效、日志接线的三条规则，以及仍未接线的已知缺口。

## 已接线的字段

- `shell`：`load()` 里未设置时取 `default_shell()`（$SHELL，否则 /bin/bash），`App::init` 用它 spawn PTY（`src/core/app.cpp` 的 `pty_.spawn(config.shell)`）。
- `log_level` / `log_file`：main() 先建 boot logger（`~/.cache/term-ime/term-ime.log`）只为让“读配置”这件事本身有地方记；读完配置后由配置接管 level 与文件。
- `rime_shared_data_dir` / `rime_user_data_dir`：空串表示“未设置”，回退逻辑在 `RimeIme` 里，配置层不猜路径。
- `active_language` / `ui_language`：分别决定输入法语种与 i18n 文案。

## 日志接线规则（2026-09-28 修正后）

1. **换目的地不许顺手关日志。** 切换 `log_file` 失败时，boot logger 仍被 spdlog 的默认指针持有、文件仍开着，报错有地方落。这里原先调 `set_level(off)`，效果是“报告失败的那条日志，把失败之后所有日志都吞掉了”。
2. **落盘时机独立于退出路径。** 文件 logger 一律 `flush_on(warn)`，再加 `spdlog::flush_every(1s)`（registry 周期刷新，对之后注册的 logger 同样生效，所以切文件后自动继承）。只依赖干净退出时的 flush，丢掉的正好是“为什么没干净退出”那几行——实测：多路复用器杀掉 pane 后日志停在启动中段。
3. **加载期的诊断随结果走。** `AppConfig::load()` 不自己调 spdlog（那一刻目的地还没定），而是把问题写进 `load_notes`，main() 在日志切换完成后以 warn 重放。`take_load_notes()` 是运行时字段，永不序列化。

## 已知未接线（本轮未修）

`show_mode_indicator`、`candidate_bar_position`、`dict_path` 三个字段目前没有任何消费者（只有 config 自身的读写与测试引用）。设置面板里出现不代表生效，接线或缺失需另开一轮。


## Timeline

- time: 2026-09-28T14:20:29
  kind: decision
  summary: "Created this page: 配置字段必须真正接线：shell 优先级、日志、rime 数据目录"
  source: "2026-09-28 代码评审修复轮"
  affects: [config-runtime-binding]

- time: 2026-09-28T19:35:44
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-28 代码评审修复轮（第 4 提交：日志与尺寸 clamp）"
  affects: [config-runtime-binding]
