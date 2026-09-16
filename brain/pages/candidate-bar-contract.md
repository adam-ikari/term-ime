---
id: candidate-bar-contract
title: "候选栏契约：显示的集合 = 可选的集合，数量随宽度自适应"
category: decision
status: active
tags: [ui, candidates, rime, config]
created: "2026-09-15T04:17:45"
updated: "2026-09-15T05:18:33"
---

<!-- compiled_truth -->
## 约束

1. **显示集合 = 可选集合**：可见候选数量由 `ui::FitCandidateBar(term_width, mode, buffer, candidates, max_items)`
   单点计算，`MainBar` 绘制与 `App::render_candidates_bar()` 的窗口切片用同一函数、同一份列表（切片后的 tail）。
   历史上 `MainBar` 自己算数量、而数字键按 rime 页索引选择，导致屏幕外候选被选中/被翻页跳过。
2. **翻页语义**：rime 单页 = `data/rime-data/default.yaml` 的 `menu/page_size: 9`（单数字键上限）。
   可见窗口由 App 的 `candidate_window_` 控制：`,`/`<`、`.`/`>` 先移动一个可见窗口，越过页尾才
   `page_up`/`page_down`，因此窄终端不会跳过候选。
3. **可配置**：`AppConfig::max_candidates`（1-9，默认 9，JSON 键 `max_candidates`，兼容旧键 `page_size`）是每页上限；
   实际数量 = min(max_candidates, 宽度能完整放下几个)。
4. **不截断**：优先显示完整候选；极端窄屏才退化为「1 个候选 + 省略号」，绝不让候选栏超出终端宽度。

## 证据（2026-09-15）

46 列 → 3 个候选；140 列 → 9 个；两种宽度下候选栏可见宽度均 ≤ 终端宽度；
窄屏连续 `.` 得到的序列 `你好/妳好/利好/立好/理好/立号/里豪/逆号/你` 与 140 列整页顺序一致（无跳过）；
窗口内按 `1` 提交的正是该窗口显示的首个候选；`max_candidates=3` → 3 个；旧键 `page_size=2` → 2 个。

## 未决

候选内容仍非纯简体：`妳好`（OpenCC t2s 无 `妳→你` 映射）以及 `楽`/`薬`（日文新字体）会出现在候选里，
与 TESTING.md §4「候选词为简体中文（无 裏/裡/妳）」不符。根因在随仓库出货的 rime 数据/OpenCC 映射，
尚未修复。


## Timeline

- time: 2026-09-15T04:17:45
  kind: decision
  summary: "Created this page: 候选栏契约：显示的集合 = 可选的集合，数量随宽度自适应"
  source: "2026-09-15 窄终端候选显示不完整修复"
  affects: [candidate-bar-contract]

- time: 2026-09-15T04:17:45
  kind: decision
  summary: "候选栏显示的候选数量随终端宽度自适应（上限 max_candidates，1-9），且显示集合必须等于数字键可选集合；跨窗口翻页不得跳过候选。"
  source: "2026-09-15 窄终端候选显示不完整修复"
  affects: [candidate-bar-contract]

- time: 2026-09-15T05:18:33
  kind: decision
  summary: "方向键(←↑/→↓)与 PgUp/PgDn 也可翻候选组；设置面板新增「候选词数量」行(1-9)"
  source: "2026-09-15 翻页键与设置项"
  affects: [candidate-bar-contract]
