---
id: candidate-paging-hint
title: "候选栏翻页提示器：‹ › 画在右端，宽度与候选同一份预算"
category: decision
status: active
tags: [ui, candidates, paging, width]
created: "2026-10-11T02:43:44"
updated: "2026-10-12T03:53:07"
---

<!-- compiled_truth -->
## 不变量（2026-10-11 起；2026-10-12 布局被用户推翻）

> 页标题里的「‹ › 画在右端」已过期：两支箭头现在**分列候选两侧**（CLI 没有改标题的命令，
> 以本段与 timeline 为准）。

候选词**两侧**有翻页提示器：`‹` 画在第一个候选前面 = 前面还有（本 rime 页里更早的窗口，或 rime 的
上一页），`›` 画在最后一个候选后面 = 后面还有。不贴行尾：宽终端上右端对齐的箭头隔着上百列 filler，
看不出属于候选栏（用户反馈原话「提示器应该显示在候选词两侧」）。
`ui::page_hint_prev_text(prev)` / `page_hint_next_text(next)` 是「画什么」和「留几列」的唯一来源
（`page_hint_width(prev, next)` = 两者宽度之和）—— App 切窗口与 MainBar 绘制两侧共用，
否则要么这一行越出终端宽度，要么预留与画出的不一致、窗口步进跳过候选。

## 箭头的列从候选里出，而「需不需要箭头」又取决于留了几列

这是个定点问题：`hint_w → fit → flags → want(hint_w)`。定点存在且能迭代求到：预留越多 fit 越小，
fit 越小「还有」越容易为真，所以 want 是 hint_w 的非减函数，取值链 `0 → 1 → 2`（`›` / `‹` / 两侧各一，
中间不再有分隔列），从 0 起迭代到收敛、`for pass < 4` 的上界留有宽余。

**历史教训仍然有效**：箭头曾被并排画在行尾（`‹ ›`，3 列），取值链 `0 → 1 → 3`——只跑两轮时 fit 按
1 列预留算、画出时却按 3 列预留，MainBar 少画一个候选而 `candidate_slots_` 仍是大值，窗口步进就跳过
一个候选（实测 46 列：`里号` 被跳过）。任何改动链长或收敛策略的人要重新想一遍这件事。

**同一个预留必须出现在 `advance_candidate_window(-1)` 的跨页落点计算里**（落在上一页的最后一个满窗口）：
那边按整页算 fit，不扣箭头列就会算出宽一个候选的窗口，回来落在用户离开的那个之前
（实测 46 列：`拟好/李浩` 那个窗口没回去）。两处共用 `bar_can_hint(cols)` 决定这宽度值不值得付。

## 谁决定画不画

App 是唯一知道「按键还能不能翻」的一侧：flags 由它算好，经
`render_candidates(..., page_prev, page_next)` 传给渲染层，渲染层不再自己推一遍。判据与
`advance_candidate_window()` 的分支判据同源 —— `‹` 是 `window > 0` 或 rime 有上一页，
`›` 是 `window + fit < all.size()` 或 rime 有下一页。提示器显示什么，按键就真能翻到什么。

## rime 侧：报而不探

rime 一次只给一页候选（`menu/page_size`），页尾列表读作「没有了」还是「在下一页」只有引擎知道。
`ImeEngine::paging()` 返回 `ImeMenuPaging{prev_page, next_page}`（rime 的 `page_no > 0` /
`!is_last_page`；没有候选时两项都为假）。探路（page_down 看列表变没变）会把页挪到用户脚下，
所以不做。rime 不给总页数，`2/5` 这种页码做不出来（用户在需求里确认要箭头、不要页码）。

宽终端是另一种形状：一个 rime 页整屏装得下，首个窗口只有跨页的 `›`，一次 `.` 直接换页，
另一半由引擎的 `page_no > 0` 补上——窗口级的 `‹` 只在窄终端出现，所以 e2e 必须两种宽度都跑。

## 退化

终端窄到 `cols - 2 < kBarHintMinCols(16)` 时不画箭头，所有列留给候选：箭头不值一个候选，
且契约是这行永不越界。极窄屏的候选截断路径不变（一个候选 + 省略号）。


## Timeline

- time: 2026-10-11T02:43:44
  kind: decision
  summary: "Created this page: 候选栏翻页提示器：‹ › 画在右端，宽度与候选同一份预算"
  source: "2026-10-11 候选栏翻页提示器"
  affects: [candidate-paging-hint]

- time: 2026-10-11T02:44:54
  kind: decision
  summary: "写入不变量：提示器与候选同一份宽度预算，宽度定点（0→1→3，三轮）两处共用，rime 报而不探"
  source: "2026-10-11 候选栏翻页提示器"
  affects: [candidate-paging-hint]

- time: 2026-10-11T02:45:47
  kind: evidence
  summary: "落地并验证：新增 e2e tests/test_candidate_paging_hint.py（46 列窄终端，17/17）、gtest ImeEngineContract.PagingReportsReachablePagesWithoutMovingTheMenu、test-ui-jsx 候选栏几何断言（该程序原先是无条件打印 All Tests Passed，现失败退出非零）。ctest 136/136、tools/run-e2e.sh 11 套全绿。实现期逮到两个真缺陷：定点只跑两轮导致窗口步进跳过候选；跨页落点不扣箭头列导致回到前一个窗口。"
  source: "2026-10-11 候选栏翻页提示器"
  affects: [candidate-paging-hint, candidate-bar-contract]

- time: 2026-10-12T03:53:07
  kind: decision
  summary: "布局推翻：‹ › 分列候选两侧（不是行尾），两者宽度相加、中间无分隔列；取值链 0→1→2"
  source: brain update-truth
  affects: [candidate-paging-hint]

- time: 2026-10-12T03:53:07
  kind: reversal
  summary: "布局推翻（用户原话「提示器应该显示在候选词两侧」）：‹ 不再与 › 并排画在行尾，改为 ‹ 在第一个候选前面、› 在最后一个候选后面。理由是宽终端上右端对齐的箭头隔着上百列 filler，读起来像屏幕噪点而不是候选栏的一部分；顺带省掉两箭头之间的分隔列，箭头宽度从 3 列降到 2 列，定点取值链由 0→1→3 变为 0→1→2（0→1→3 那次跳候选的历史教训与 for pass<4 的宽余上界保留）。同步：page_hint_text 拆成 page_hint_prev_text/page_hint_next_text，MainBar 改为在候选序列前后各插一个 Text；单测改为几何断言（‹ 在 1. 之前、› 在末位候选之后，末位按 FitCandidateBar 实际 fit 的个数取，46 列两箭头下只有 2 个候选放得下）；e2e 新增两条 flanking 断言与 120 列/resize 用例，共 25 项，ctest 136、run-e2e 11 套全绿。"
  source: "2026-10-12 用户反馈"
  affects: [candidate-paging-hint, candidate-bar-contract]
