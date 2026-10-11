---
id: candidate-paging-hint
title: "候选栏翻页提示器：‹ › 画在右端，宽度与候选同一份预算"
category: decision
status: active
tags: [ui, candidates, paging, width]
created: "2026-10-11T02:43:44"
updated: "2026-10-11T02:45:47"
---

<!-- compiled_truth -->
## 不变量（2026-10-11 起）

候选栏右端有**翻页提示器**：`‹` = 前面还有（本 rime 页里更早的窗口，或 rime 的上一页），
`›` = 后面还有，两边都能翻时画 `‹ ›`。`ui::page_hint_text(prev, next)` 是「画什么」和「留几列」
的唯一来源（`page_hint_width` = 它的显示宽度）—— App 切窗口与 MainBar 绘制两侧共用，
否则要么这一行越出终端宽度，要么预留比画出的多一列、窗口步进跳过候选。

## 箭头的列从候选里出，而「需不需要箭头」又取决于留了几列

这是个定点问题：`hint_w → fit → flags → want(hint_w)`。定点存在且能迭代求到：预留越多 fit 越小，
fit 越小「还有」越容易为真，所以 want 是 hint_w 的非减函数，取值链 `0 → 1 → 3`（`›` / `‹` / `‹ ›`），
从 0 起最多三轮收敛（`for pass < 4`，收敛后 `break`）。**两轮不够**：`0 → 1 → 3` 恰好是要三轮的
那种链，只跑两轮时 fit 是按 1 列预留算的、画出时却按 3 列预留，MainBar 少画一个候选而
`candidate_slots_` 仍是大值，窗口步进就跳过一个候选（实测 46 列：`里号` 被跳过）。

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

## 退化

终端窄到 `cols - 3 < kBarHintMinCols(16)` 时不画箭头，所有列留给候选：箭头不值一个候选，
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
