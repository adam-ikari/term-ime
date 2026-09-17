---
id: passthrough-query-architecture
title: "查询由真实终端应答:term-ime 不自答(反自建响应机制)"
category: decision
status: active
tags: [terminal, parser, architecture, query]
created: "2026-09-17T15:26:24"
updated: "2026-09-17T15:26:40"
---

<!-- compiled_truth -->
## 事实

term-ime 是 passthrough 架构:PTY 输出经 on_pty_data 原样 forward_output 到真实终端(stdout),真实终端是查询的权威回答者;shell 收到的回答经 stdin → InputProcessor → pty_.write 回流。

因此以下已天然实现(实证 4/4 PASS):
- OSC 透传:标题 `ESC]0;...`、超链接 `ESC]8;;...`、剪贴板、颜色查询 OSC 4/10/11
- SGR/DSR 光标位置报告 `ESC[6n` / `ESC[?6n` 往返
- DA1 设备属性 `ESC[c` 转发

## 约束

1. **不要自建查询响应机制**。term-ime 如果自己回 DSR/OSC 查询,会与真实终端双重应答、产生冲突。查询只透传。
2. parser 的 OSC/私有 CSI swallow 只是**不写内部网格**(Screen grid 不含控制序列),不代表丢弃——字节仍经 forward_output 原样到真实终端。
3. 已知限制(设计如此,非 bug):settings 面板打开时 on_pty_data 只 feed parser 不 forward_output(面板是覆盖层,不该被 shell 输出污染),shell 查询会重发、不真卡死。文档化即可,不写代码。
4. 潜在加固(到不了必做):IME composing 状态下 app.cpp:403-406 会丢非方向键 escape(含查询响应)。仅当存在 IME 激活时 shell 前台查询的真实场景才做。

## 为什么记录

防止后人重复勘察、误以为需自答查询,或把"面板期间不转发"当成 bug 去改。


## Timeline

- time: 2026-09-17T15:26:24
  kind: decision
  summary: "Created this page: 查询由真实终端应答:term-ime 不自答(反自建响应机制)"
  source: "2026-09-16 M3 勘察"
  affects: [passthrough-query-architecture]

- time: 2026-09-17T15:26:40
  kind: decision
  summary: "记录 passthrough 查询架构:查询由真实终端应答,term-ime 不自答(反自建响应机制)"
  source: "2026-09-16 M3 勘察"
  affects: [passthrough-query-architecture]
