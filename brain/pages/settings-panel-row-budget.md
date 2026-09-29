---
id: settings-panel-row-budget
title: "设置面板高度预算：一行描述 + 分组标题，面板 19 行钉死 20 行终端"
category: decision
status: active
tags: [ui, settings, i18n, ftxui]
created: "2026-09-29T03:20:44"
updated: "2026-09-29T05:17:20"
---

<!-- compiled_truth -->
**结论**：设置面板的内容高度是 **19 行**（7 项 + 1 分组标题 + 1 描述行 + 标题/分隔/提示/关闭），FTXUI 超预算只裁不折，先被裁的是最底下的「关闭」，面板打不开设置。所以任何往面板加行的动作都要同时把这 19 行守住，或者把 `tests/test_settings.cpp` 的 Test 7 预算断言一起改。

**描述的显示规则（固定槽位）**：`SettingsItem.description`（`src/ui/settings.hpp`）画在**所有项之后、分隔线之前**的单一固定行，永远占一行，内容是焦点项的 desc（焦点在「关闭」时是 `settings.close.desc`）。v1.1.6 之前是「焦点项正下方一行随焦点走」；改成固定槽是因为模糊音加了 `window` 框 —— 描述若画在框内，框高会随焦点 5↔6 行，撑爆预算。Test 7 只断描述文本可见、不断位置。

**分组是 ftxui `window` 框，不是裸标题行**：`SettingsItem.group_header` 非空标记分组**起点**（目前只有第一项模糊音带「模糊音」），从该 item 起到 items 尾部收进一个带标题的 `window`（`╭模糊音──╮…╰──╯`）。分组不新增可选项：`focus_index` 与 `settings_handle_key` 的语义不变，e2e「按 5 次 ↓ 走到关闭」照旧成立。模糊音必须是 items 尾部 —— 渲染循环遇 `group_header` 起框、遇到尾收框，中间不允许混进非组成员。

**宽度上限 52 显示列**：描述是一条不折行的 `Text`，超宽会吃掉右边框。`tests/test_i18n.cpp` 的 `SettingsDescriptionsResolveAndFit` 对 zh/en、对内置表与 `data/translations` 两种来源都断言 `utf8::string_width <= 52`，并把 60×20 的渲染留白守住。

**文案约定**：模糊音五项的 label 是音标对（zh⇄z / n⇄l / r⇄l / hu⇄fu / an⇄ang），组名由 `window` 框标题承担，不要在 label 里重复「模糊音 」。r 组标 `r⇄l` 只是主对（引擎实际 r→l 和 r→y 两段单向推导，见 schema 的 `fuzzy:r_l`/`fuzzy:r_y`）；hu_f 标 `hu⇄fu` 点名真实范围。描述统一写「不区分 X 与 Y」（en：`Treats X and Y alike`），只说覆盖范围，不带例子、不讲代价 —— 例子和两处边界（r 系只认单方向；h/f 实际只做 hu↔fu、hong↔feng、hun↔fen、hua/fa 几对，`han`/`fan` 不互认）写在 `website/docs/fuzzy.md`。

**提示区**：底部三行按键提示并成一行 `选择 ↑↓   切换 ←→/Enter   取消 Esc/Tab`。合并是为了换出描述行的预算。面板是否打开/关闭的指纹因此从 `Up/Down` 改成 `Esc/Tab`（`tests/test_settings_e2e.py`、`tests/test_settings_panel_e2e.py` 的 `panel_gone`），这个字面量别再动。

**验证位置**：`tests/test_settings.cpp` Test 7 逐焦点渲染 60×20，断言底部边框在、「关闭」在、当前项描述在、盒子行数不随焦点变化；CI 用 `./test-settings` 跑（非 0 退出即失败）。


## Timeline
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-29 设置面板描述改造会话"
  affects: [settings-panel-row-budget]


## Timeline

- time: 2026-09-29T03:20:44
  kind: decision
  summary: "Created this page: 设置面板高度预算：一行描述 + 分组标题，面板 19 行钉死 20 行终端"
  source: created via brain create-page
  affects: [settings-panel-row-budget]

- time: 2026-09-29T03:21:00
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-29 设置面板描述改造会话"
  affects: [settings-panel-row-budget]

- time: 2026-09-29T05:17:20
  kind: decision
  summary: "v1.1.6：模糊音改成 window 框 + label 改音标对 + 描述改固定槽"
  source: "2026-09-29 模糊音分组/label 改造会话"
  affects: [settings-panel-row-budget]
