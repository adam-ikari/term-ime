---
id: candidate-bar-contract
title: "候选栏契约：显示的集合 = 可选的集合，数量随宽度自适应"
category: decision
status: active
tags: [ui, candidates, rime, config]
created: "2026-09-15T04:17:45"
updated: "2026-10-12T05:02:19"
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
5. **翻页提示器的宽度也在这份预算里**：`‹` 画在第一个候选前面、`›` 画在最后一个候选后面（候选两侧，
   不是行尾），两侧是**常驻槽位**——没有可翻的方向时那一列留白，所以预留恒为
   `ui::page_hint_width()` = 2 列、与 flags 无关，候选词不会因箭头出现/消失而左右平移；
   调用方（App 切窗口、`advance_candidate_window` 跨页落点）与 `MainBar` 用同一对函数
   `ui::page_hint_prev_text()` / `page_hint_next_text()` 预留。解法见 [[candidate-paging-hint]]。
   约束 1 因此多了一条推论：**预留列变化后窗口步进必须跟着变**，否则 `candidate_slots_`
   大于实际画出的数量，翻页会跳过候选。
6. **这一行里的位置不许漂**：提示器槽位常驻（约束 5）+ 拼音区的分隔符以第一个候选词为准
   （见 [[pinyin-preedit-delimiters]]），两者合起来保证同一组合输入下 `"1."` 的列位置在所有窗口里相同——
   用户按 `1.` 选中的永远是屏幕上的那个词。

## 证据（2026-09-15）

46 列 → 3 个候选；140 列 → 9 个；两种宽度下候选栏可见宽度均 ≤ 终端宽度；
窄屏连续 `.` 得到的序列 `你好/妳好/利好/立好/理好/立号/里豪/逆号/你` 与 140 列整页顺序一致（无跳过）；
窗口内按 `1` 提交的正是该窗口显示的首个候选；`max_candidates=3` → 3 个；旧键 `page_size=2` → 2 个。

## 证据（2026-10-11，提示器加入后）

46 列下每帧候选栏宽 46 列（含箭头），窗口序列 `你好/利好/立好 → 理好/立号 → 里豪/逆号 → 拟好/李浩`
连续无跳过，跨 rime 页后 `,` 回到 `拟好/李浩` 并在该窗口按 `1` 上屏该窗口首个候选。
`tests/test_candidate_paging_hint.py` 17/17；`test_candidate_paging.py` 3/3、`test_fuzzy_pinyin.py` 回归通过。

## 证据（2026-10-12，箭头移到候选两侧 + 槽位常驻）

46 列实测行：`[拼] ni hao ‹ 1.逆号 2.拟好 3.李浩 ›`——`‹` 紧贴第一个候选、`›` 紧贴末位候选，
其后才是 filler。槽位常驻后，同一组合输入下 `"1."` 的列偏移在所有窗口里都是 15（实测），
且整行宽恒 46 列；120 列（整页 9 候选装得下）`[拼] ni hao 1.逆号 … 9.李浩 ›` 只有跨页的 `›`，
按一次 `.` 换页后 `‹ 1.你 … 9.利 ›`，另一半由 rime 的 `page_no > 0` 补上；resize 120↔46 往返后重算，行不越界。
拼音区的分隔符随锚定不再变形（单字页也是 `ni hao`，此前是 `nihao`）。
`tests/test_candidate_paging_hint.py` 27/27、`test_ui_jsx.cpp` 9 项几何断言（含「把箭头抹成空格后与无箭头那行逐字节相同」）、
`ctest` 136/136、`tools/run-e2e.sh` 11 套 130 项全绿。

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

- time: 2026-10-11T02:46:18
  kind: decision
  summary: "补约束 5：翻页提示器与候选共用一份宽度预算，预留变化后窗口步进必须跟着变；补 2026-10-11 证据"
  source: "2026-10-11 候选栏翻页提示器"
  affects: [candidate-bar-contract]

- time: 2026-10-12T03:54:57
  kind: decision
  summary: "约束 5 改写：‹ › 分列候选两侧，宽度相加，改用 page_hint_prev_text/page_hint_next_text 预留；补 2026-10-12 证据"
  source: brain update-truth
  affects: [candidate-bar-contract]

- time: 2026-10-12T03:54:57
  kind: decision
  summary: "约束 5 随布局推翻同步改写（详见 [[candidate-paging-hint]] 的 reversal）：箭头不再并排在行尾，改为 ‹ 在首个候选前、› 在末位候选后，两列宽度相加；预留函数由 page_hint_text 拆为 page_hint_prev_text/page_hint_next_text。补 2026-10-12 证据：46 列实测行 '[拼] ni hao ‹ 1.逆号 2.拟好 3.李浩 ›'、120 列整页只余跨页 ›、按 . 换页后另一半由 page_no>0 补上、resize 往返后 fit 与箭头一起重算；ctest 136、run-e2e 11 套全绿。"
  source: "2026-10-12 翻页提示器移到候选两侧"
  affects: [candidate-bar-contract]

- time: 2026-10-12T05:02:19
  kind: decision
  summary: "约束 5 改写：提示器槽位常驻、预留恒 2 列；新增约束 6『这一行里的位置不许漂』；补 2026-10-12 证据"
  source: brain update-truth
  affects: [candidate-bar-contract]

- time: 2026-10-12T05:02:19
  kind: decision
  summary: "约束 5 随槽位常驻改写（预留恒为 ui::page_hint_width()=2，与 flags 无关），并新增约束 6：槽位常驻 + 拼音区分隔符锚定首候选，合起来保证同一组合下 '1.' 的列位置在所有窗口相同（实测 46 列偏移恒为 15）——这是『显示集合 = 可选集合』在列位置上的对应物。证据补 2026-10-12：ctest 136、run-e2e 11 套 130 项全绿。"
  source: "2026-10-12 槽位常驻 + 拼音区锚定"
  affects: [candidate-bar-contract, pinyin-preedit-delimiters]
