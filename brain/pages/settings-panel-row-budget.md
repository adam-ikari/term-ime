---
id: settings-panel-row-budget
title: "设置面板高度预算：一行描述 + 分组标题，面板 19 行钉死 20 行终端"
category: decision
status: active
tags: [ui, settings, i18n, ftxui]
created: "2026-09-29T03:20:44"
updated: "2026-09-29T03:21:00"
---

<!-- compiled_truth -->
**结论**：设置面板的内容高度是 **19 行**（7 项 + 1 分组标题 + 1 描述行 + 标题/分隔/提示/关闭），FTXUI 超预算只裁不折，先被裁的是最底下的「关闭」，面板打不开设置。所以任何往面板加行的动作都要同时把这 19 行守住，或者把 `tests/test_settings.cpp` 的 Test 7 预算断言一起改。

**描述的显示规则**：`SettingsItem.description`（`src/ui/settings.hpp`）只在**焦点项**那一行下面渲染一行，且无论焦点在哪（含「关闭」）**永远占一行**。少一行多一行都会让下面的行整体上跳，焦点走到「关闭」时必须显示 `settings.close.desc` 补这个槽位。

**分组标题不是 item**：`SettingsItem.group_header` 非空表示在这一行前面画一行标题（目前只有第一项模糊音带「模糊音」）。它不进 `state.items`、不可选中，`focus_index` 与 `settings_handle_key` 的语义不变 —— 现有 e2e 里「按 5 次 ↓ 走到关闭」因此照旧成立。

**宽度上限 52 显示列**：描述是一条不折行的 `Text`，超宽会吃掉右边框。`tests/test_i18n.cpp` 的 `SettingsDescriptionsResolveAndFit` 对 zh/en、对内置表与 `data/translations` 两种来源都断言 `utf8::string_width <= 52`，并把 60×20 的渲染留白守住。

**文案约定**：模糊音五项的 label 是短标签（平翘舌 / n/l / r 系 / h/f / 前后鼻音），组名由分组标题承担，不要在 label 里重复「模糊音 」。描述统一写「不区分 X 与 Y」（en：`Treats X and Y alike`），只说覆盖范围，不带例子、不讲代价 —— 例子和两处边界（r 系只认单方向；h/f 实际只做 hu↔fu、hong↔feng、hun↔fen、hua/fa 几对，`han`/`fan` 不互认）写在 `website/docs/fuzzy.md`。

**提示区**：底部三行按键提示并成一行 `选择 ↑↓   切换 ←→/Enter   取消 Esc/Tab`。合并是为了换出描述行的预算。面板是否打开/关闭的指纹因此从 `Up/Down` 改成 `Esc/Tab`（`tests/test_settings_e2e.py`、`tests/test_settings_panel_e2e.py` 的 `panel_gone`），这个字面量别再动。

**验证位置**：`tests/test_settings.cpp` Test 7 逐焦点渲染 60×20，断言底部边框在、「关闭」在、当前项描述在、盒子行数不随焦点变化；CI 用 `./test-settings` 跑（非 0 退出即失败）。


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
