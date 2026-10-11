---
id: review-fix-wide-char
title: "评审修复轮:宽字符右半格契约 + 参数化 CSI + 组合态透传"
category: decision
status: active
tags: [parser, renderer, review, wide-char]
created: "2026-09-18T06:01:51"
updated: "2026-10-10T18:33:04"
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

- time: 2026-10-10T11:44:25
  kind: reversal
  summary: "评审复核：本页约束 1 的「整格清除」只在擦除路径落地了一半。实测（2026-10-10 复核，非新评审轮）：Screen::put 写入窄字符落在宽字符右半格时不左清邻格，redraw_shell 见 wide=true 即 ++c 跳过该列，导致写入的字符永不上屏；Screen::erase_* 同理只清半格。即约束 1 目前对 put/erase 均未真正成立，只有写入宽字符时标记右半格那半是实现的。"
  source: "2026-10-10 全量代码评审复核"
  affects: [review-fix-wide-char]

- time: 2026-10-10T17:33:51
  kind: evidence
  summary: "约束 1 的缺口补齐：src/terminal/screen.cpp 增加 is_wide_left/cell_is_wide_left/is_wide_right/expand_wide_range；Screen::put 写入窄字符压在宽字符任一半格时把整个 pair（含残留右半）一起退掉，erase_line/erase_cells/erase_display 先按 wide 邻格扩范围再清，Screen::resize 变窄时把被切开的左半单独清空。tests/test_utf8.cpp 新增 7 个用例（窄压右半/窄压左半/宽压宽/局部擦不拆 pair/行内擦从右半清整对/行内擦穿左半达右半/变窄留下孤右半）。全量 ctest 135/135 绿。"
  source: "2026-10-10 修复轮"
  affects: [review-fix-wide-char]

- time: 2026-10-10T18:33:04
  kind: evidence
  summary: "约束 1 现在有 e2e 覆盖：tests/test_wide_pair.py 在真 PTY 里驱动 shell 打印 你好，再用 \\b 把窄字符压到右半格（以及 \\b + ESC[K 从右半格擦除），然后开设置面板整屏覆盖、按 ESC 关闭触发 redraw_shell，断言重绘帧里 Z 画得出来且 好 不再出现、擦除后 好 消失而 你 保留。判别力：对仅回退 src/terminal/screen.cpp 的二进制 1/4 通过，对修复后 4/4。原始输出层看不到这个缺陷——parser 重放字节，缺陷只在重绘时暴露，所以本用例必须走「覆盖层→关闭」这条路径。"
  source: 2026-10-11
  affects: [review-fix-wide-char]
