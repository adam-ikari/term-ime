---
id: candidate-paging-hint
title: "候选栏翻页提示器：‹ › 画在右端，宽度与候选同一份预算"
category: decision
status: active
tags: [ui, candidates, paging, width]
created: "2026-10-11T02:43:44"
updated: "2026-10-12T05:02:19"
---

<!-- compiled_truth -->
## 不变量（2026-10-11 起；2026-10-12 两度被用户推翻）

> 页标题里的「‹ › 画在右端」已过期：两支箭头现在**分列候选两侧**（CLI 没有改标题的命令，
> 以本段与 timeline 为准）。2026-10-12 又推翻一次：槽位**不显示也要占位**。

候选词**两侧**有翻页提示器：`‹` 画在第一个候选前面 = 前面还有（本 rime 页里更早的窗口，或 rime 的上一页），
`›` 画在最后一个候选后面 = 后面还有。不贴行尾：宽终端上右端对齐的箭头隔着上百列 filler，
看不出属于候选栏（用户反馈原话「提示器应该显示在候选词两侧」）。

**两侧都是常驻槽位**（用户原话「提示器不显示也要占位」）：没有可翻的方向时那一列留白。
两个后果：一是候选词不会因为箭头出现/消失而左右平移——`1.` 永远指着它刚才指着的那个词；
二是宽度预留变成常数（`ui::page_hint_width()` = 2），不再依赖 flags，定点迭代整个消失。

`ui::page_hint_prev_text(prev)` / `page_hint_next_text(next)` 是「画什么」和「留几列」的唯一来源
（箭头形与留白形同宽，宽度相加 = 2）—— App 切窗口与 MainBar 绘制两侧共用，
否则要么这一行越出终端宽度，要么预留与画出的不一致、窗口步进跳过候选。

## 曾经的定点：已随「恒定预留」消失（历史教训保留）

提示器刚加入时预留是**按 flags 反推**的：`hint_w → fit → flags → want(hint_w)`，取值链
`0 → 1 → 3`（并排在行尾时）/ `0 → 1 → 2`（移到候选两侧后），要迭代到收敛、循环上界必须留宽余。
**两轮不够**：`0 → 1 → 3` 恰好是要三轮的那种链，只跑两轮时 fit 是按 1 列预留算的、画出时却按 3 列预留，
MainBar 少画一个候选而 `candidate_slots_` 仍是大值，窗口步进就跳过一个候选（实测 46 列：`里号` 被跳过）。

现在预留恒为 `ui::page_hint_width()`（两侧各 1 列），fit 一次算完，没有可收敛的循环、也没有可设错的上界。
**同一个预留仍必须出现在 `advance_candidate_window(-1)` 的跨页落点计算里**（落在上一页的最后一个满窗口）：
那边按整页算 fit，不扣槽位列就会算出宽一个候选的窗口，回来落在用户离开的那个之前
（实测 46 列：`拟好/李浩` 那个窗口没回去）。

## 谁决定画不画

App 是唯一知道「按键还能不能翻」的一侧：flags 由它算好，经
`render_candidates(..., page_prev, page_next)` 传给渲染层，渲染层不再自己推一遍。判据与
`advance_candidate_window()` 的分支判据同源 —— `‹` 是 `window > 0` 或 rime 有上一页，
`›` 是 `window + fit < all.size()` 或 rime 有下一页。提示器显示什么，按键就真能翻到什么。
flags 只决定画不画箭头，**不决定占不占列**。

## rime 侧：报而不探

rime 一次只给一页候选（`menu/page_size`），页尾列表读作「没有了」还是「在下一页」只有引擎知道。
`ImeEngine::paging()` 返回 `ImeMenuPaging{prev_page, next_page}`（rime 的 `page_no > 0` /
`!is_last_page`；没有候选时两项都为假）。探路（page_down 看列表变没变）会把页挪到用户脚下，
所以不做。rime 不给总页数，`2/5` 这种页码做不出来（用户在需求里确认要箭头、不要页码）。

宽终端是另一种形状：一个 rime 页整屏装得下，首个窗口只有跨页的 `›`，一次 `.` 直接换页，
另一半由引擎的 `page_no > 0` 补上——窗口级的 `‹` 只在窄终端出现，所以 e2e 必须两种宽度都跑。

## 退化

极窄屏不再有「不画箭头」的阈值（`kBarHintMinCols` 已删）：槽位恒占 2 列，箭头本身是免费的
（它替代的就是那一列留白）。极端窄屏的候选截断路径不变（一个候选 + 省略号）。


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

- time: 2026-10-12T05:02:19
  kind: decision
  summary: "槽位常驻（不显示也占位）：预留恒为 2 列，定点迭代删除；‹ › 分列候选两侧"
  source: brain update-truth
  affects: [candidate-paging-hint]

- time: 2026-10-12T05:02:19
  kind: reversal
  summary: "再次推翻（用户原话「提示器不显示也要占位」）：两侧改为常驻槽位，没有可翻的方向时那一列留白。后果有二：候选词不再因箭头出现/消失而左右平移（'1.' 永远指着同一个词）；宽度预留从『按 flags 反推』的定点变成常数 ui::page_hint_width()=2，定点迭代整个删除——历史上那两个缺陷（只跑两轮跳过『里号』、跨页落点不扣箭头列）都源于那个反推，现在结构上不可能再出现；仍需注意的是 advance_candidate_window(-1) 的落点计算必须用同一个常数。kBarHintMinCols 阈值随之删除：槽位恒占，箭头本身免费。测试：单测新增『把箭头抹成空格后与无箭头那行逐字节相同』，e2e 新增『所有窗口 1. 列偏移相同』（实测 15）。"
  source: "2026-10-12 用户反馈"
  affects: [candidate-paging-hint, candidate-bar-contract]
