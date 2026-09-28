---
id: parser-stream-state-contract
title: "Parser 流式契约：未完成 UTF-8 用 pending_ 跨 feed 保留，私有 CSI/OSC 不写网格"
category: concept
status: active
tags: [parser, utf8, csi, osc]
created: "2026-09-14T15:18:22"
updated: "2026-09-28T16:49:22"
---

<!-- compiled_truth -->
`Parser` 的输入是**任意切分的字节流**——PTY 的 read 边界与转义序列边界无关。这条契约定义了三个不变式。

## 不变式 1：UTF-8 序列可跨 `feed()` 边界

- 读到不完整的 UTF-8 序列时，字节存入 `pending_`，**不**当作 LATIN-1 逐字节上屏。
- 下次 `feed()` 先拼接 `pending_` 再解析；只有序列完整才产出一个宽字符。
- 违反了会看到一个多字节字符被拆成两个乱码字形。

## 不变式 2：不渲染的控制序列不进网格

- 私有参数 CSI（`ESC [ ?` 等 private-parameter 形式，如 `ESC[?25h` 光标显隐、`ESC[?1049h` 备用屏）只更新 Parser/Screen 的模式状态，**不写任何字符到屏幕网格**。
- OSC 字符串（`ESC ] …` 到 `BEL` / `ST`）的**字符串体**一律吞掉，不进网格；只有需要被 shell/终端状态消费的部分才处理。
- CSI 的终止符按 **ECMA-48** 判定（`0x40–0x7E` 区间字节结束序列），不要只匹配少数几个常见字母。
- 高位字节（`>=0x80`）只有在 Normal 状态才是文本；序列内部它是 CSI 的中断或 OSC 的正文（CJK 窗口标题最常见）。判据写作 `byte < 0x80 || state_ != State::Normal`。

## 不变式 3：转义序列的终止码不是文本

- ESC 之后落在 `0x20–0x2F` 的字节是 **ECMA-48 中间码**：`(` `)` `*` `+` `-` `.` `/`（字符集指定 SCS 与 RFC 1479）、`#`（DECALN/DECSCA）、`%`（ISO 2022 选择）。它们后面**还有一个终止码**，而终止码本身是普通可打印字符（`B` = US ASCII，`0` = DEC 线形图）。
- 所以 Escape 状态见到中间码必须进 `State::Scs`，不能回 Normal：回 Normal 等于把终止码直接写上网格。ncurses 程序（htop、vim）几乎每帧都发 `ESC(B`，泄漏的形态是影子网格里长出杂散的 `B`/`0`，`redraw_shell()` 再把它们画回真终端，于是全屏 TUI 在遮罩关闭后整体错位。
- 多中间码合法（`ESC ( / B`），故 Scs 内 `0x20–0x2F` 继续等待、其余字节即为终止码并结束；序列中途再来一个 ESC 则重新开始（不把 ESC 当终止码吞掉）。
- DECSC/DECRC（`ESC 7` / `ESC 8`）与 `CSI s` / `CSI u` 共用同一个保存槽，与 xterm 一致。

## 反面例子

把 `ESC[?25l` 的参数当普通字符上屏，会在网格里留下 `?25l` 之类垃圾；只认 `m`/`H`/`J` 作为 CSI 终止符，会让参数较长的合法序列被截断误解析；把 `ESC(B` 的 `B` 当文本上屏，会让"遮罩关闭后恢复 shell 视图"这件本来确定的事变成往屏幕上撒字符。

## 测试字面量的坑

C++ 的十六进制转义是贪婪的：`"\x1b7"` 是 U+01B7 而不是 ESC + '7'，`"\x1bA"` 同理。写转义序列测试时必须拆成 `"\x1b" "7"`。测试里若出现 `U+FFFD`，先怀疑字面量而不是解析器。

## 影响面

`src/terminal/parser.{hpp,cpp}`、`src/terminal/screen.cpp`；回归覆盖在 `tests/test_utf8.cpp`。相关：[[status-bar-owns-last-row]]、[[review-fix-wide-char]]。


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

- time: 2026-09-28T14:19:48
  kind: decision
  summary: "不变式 2 补一条判据：高位字节（>=0x80）只有在 Normal 状态才是文本。feed() 里必须写作 byte < 0x80 || state_ != State::Normal 才交给 handle_char，否则 ESC]0;中文 BEL 这类 CJK 窗口标题会被当 UTF-8 解码写进网格，且残留在 CSI 状态里的终止符（如 m）会被误当作 SGR 执行改色。PTY 输出是不可信输入。"
  source: "2026-09-28 代码评审修复轮"
  affects: [parser-stream-state-contract]

- time: 2026-09-28T16:47:07
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-29 SCS 子态修复（评审 B1）"
  affects: [parser-stream-state-contract]

- time: 2026-09-28T16:47:47
  kind: decision
  summary: "不变式 3 落地：Escape 见 0x20–0x2F 中间码进 State::Scs。同时收敛一条范围判断——上一轮把 htop 遮罩关闭后的乱象归因于'影子模型没建模 DEC 私有模式（1049 备用屏、2004 括号粘贴）'，实测归因错了：htop -d 60（自刷机关掉，屏幕内容只可能来自我们的 redraw_shell）在修复前后对比，乱象来源是 ESC(B 的终止码 B 被当文本写进网格，SCS 子态修完后 redraw_shell 输出与基线逐字节一致。因此 1049/备用屏双网格建模仍然不做（没有症状要求它），备用屏相关的只是'遮罩期间输出不转发'这一条既有行为。2004 是另一回事：它不是渲染问题而是输入协议问题——子进程开了括号粘贴，term-ime 的粘贴路径却不发 ESC[200~/ESC[201~，属于粘贴那条提交的范围。"
  source: "2026-09-29 评审第 2 轮修复（B1 落地，B2 范围收敛）"
  affects: [parser-stream-state-contract]

- time: 2026-09-28T16:49:22
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-29 SCS 子态修复（评审 B1）；修悬挂链接"
  affects: [parser-stream-state-contract]
