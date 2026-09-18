---
id: review-fix-wide-char
title: "评审修复轮:宽字符右半格契约 + 参数化 CSI + 组合态透传"
category: decision
status: active
tags: [parser, renderer, review, wide-char]
created: "2026-09-18T06:01:51"
updated: "2026-09-18T06:08:50"
---

<!-- compiled_truth -->
评审修复轮(4 commits):宽字符占右半格+参数化光标 CSI 上限+中文组合态透传控制字节+Pty::write 总上限。

## 约束

1. **宽字符右半格契约**:写入 CJK 等宽字符(宽=2)时,右半格必须标记为宽尾(continuation);重绘/局部刷新不得把两格拆开渲染;光标或擦除落在宽字符中间时必须整格清除(连左半一起清),否则出现半字残影。
2. **参数化光标移动**:CUP/HVP 支持任意行列参数(非只 1;1);CSI 参数个数设上限(防恶意转义序列炸内存)。
3. **组合态透传**:IME 组合态下 Ctrl+C/D/Z 必须透传给 shell(不能被 IME 吞掉);控制字节过滤职责在输入状态机层,不在 IME 层。
4. **Pty::write 阻塞上限**:写 PTY 阻塞时必须有总字节上限,防止 shell 不读时无限缓冲死循环。
5. **redraw 不做光标定位**:全量重绘前不要发绝对光标定位(会闪烁),靠完整重绘覆盖。
6. **bar_dirty 清零时机**:状态栏脏标记在扫描重绘后立即清零,不能留到下一帧(否则重复重绘)。

## 证据

tests/test_utf8.cpp 新增宽字符右半格/参数化光标/Tab 用例;test_input_processor.cpp 新增组合态透传用例。全量回归通过(58 gtest + e2e)。


## Timeline

- time: 2026-09-18T06:01:51
  kind: decision
  summary: "Created this page: 评审修复轮:宽字符右半格契约 + 参数化 CSI + 组合态透传"
  source: "2026-09-18 代码评审修复轮"
  affects: [review-fix-wide-char]

- time: 2026-09-18T06:07:11
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-18 代码评审修复轮"
  affects: [review-fix-wide-char]

- time: 2026-09-18T06:08:50
  kind: decision
  summary: "评审驱动修复 4 轮落地"
  source: "2026-09-18 代码评审修复轮"
  affects: [review-fix-wide-char]
