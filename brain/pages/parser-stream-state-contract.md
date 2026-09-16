---
id: parser-stream-state-contract
title: "Parser 流式契约：未完成 UTF-8 用 pending_ 跨 feed 保留，私有 CSI/OSC 不写网格"
category: concept
status: active
tags: [parser, utf8, csi, osc]
created: "2026-09-14T15:18:22"
updated: "2026-09-14T15:18:22"
---

<!-- compiled_truth -->
`Parser` 的输入是**任意切分的字节流**——PTY 的 read 边界与转义序列边界无关。这条契约定义了两个不变式。

## 不变式 1：UTF-8 序列可跨 `feed()` 边界

- 读到不完整的 UTF-8 序列时，字节存入 `pending_`，**不**当作 LATIN-1 逐字节上屏。
- 下次 `feed()` 先拼接 `pending_` 再解析；只有序列完整才产出一个宽字符。
- 违反了会看到一个多字节字符被拆成两个乱码字形。

## 不变式 2：不渲染的控制序列不进网格

- 私有参数 CSI（`ESC [ ?` 等 private-parameter 形式，如 `ESC[?25h` 光标显隐、`ESC[?1049h` 备用屏）只更新 Parser/Screen 的模式状态，**不写任何字符到屏幕网格**。
- OSC 字符串（`ESC ] …` 到 `BEL` / `ST`）的**字符串体**一律吞掉，不进网格；只有需要被 shell/终端状态消费的部分才处理。
- CSI 的终止符按 **ECMA-48** 判定（`0x40–0x7E` 区间字节结束序列），不要只匹配少数几个常见字母。

## 反面例子

把 `ESC[?25l` 的参数当普通字符上屏，会在网格里留下 `?25l` 之类垃圾；只认 `m`/`H`/`J` 作为 CSI 终止符，会让参数较长的合法序列被截断误解析。

## 影响面

`src/terminal/parser.{hpp,cpp}`、`src/terminal/screen.cpp`；回归覆盖在 `tests/test_utf8.cpp`。相关：[[status-bar-owns-last-row]]。


## Timeline

- time: 2026-09-14T15:18:22
  kind: decision
  summary: "Created this page: Parser 流式契约：未完成 UTF-8 用 pending_ 跨 feed 保留，私有 CSI/OSC 不写网格"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [parser-stream-state-contract]

- time: 2026-09-14T15:18:22
  kind: decision
  summary: "Parser::feed 可被任意切分：未完成 UTF-8 序列存 pending_ 跨调用；私有参数 CSI（ESC[?…）与 OSC 字符串体一律不写屏幕网格"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [parser-stream-state-contract]
