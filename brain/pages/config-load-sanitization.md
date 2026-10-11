---
id: config-load-sanitization
title: "配置加载侧消毒：越界钳制、坏类型按缺省（不丢弃整份文件）"
category: decision
status: active
tags: [config, robustness, rime]
created: "2026-09-16T05:03:10"
updated: "2026-10-10T16:44:31"
---

<!-- compiled_truth -->
配置加载侧消毒原则：**越界钳制、坏类型按缺省，且绝不丢弃整份文件**。

## 为什么必须在读取处判类型，而不是靠 try/catch

`nlohmann::json::value(key, default)` 在「键存在但类型不符」时抛 `type_error.302`，
而 `AppConfig::load()` 把任何异常都报成「config failed to load → 整套默认值」。
于是**一个写错的键会带走用户所有真正配置过的键**。所以类型预检不是洁癖，是这条
失败路径的唯一止血点；`from_json` 里现在不存在 `value()` 调用。

## 每个键的消毒口径（`src/core/config.cpp` `from_json`）

- `max_candidates`：钳制到 [1,9]（单数字选择键的合法范围）；非整数按缺省 9；兼容旧键 `page_size`。
- `log_level`：枚举 debug/info/warn/error，其余（含坏类型）按缺省 "warn"。
- `candidate_bar_position`：枚举 bottom/top，其余按缺省 "bottom"。
- 字符串键 `shell` / `active_language` / `ui_language` / `rime_shared_data_dir` /
  `rime_user_data_dir` / `log_file`：非字符串按缺省。
- 布尔键 `show_mode_indicator` 与遗留 `fuzzy_pinyin`：非布尔按缺省（true=全开）。
- 数组 `fuzzy_groups`：**逐元素**判字符串，坏元素丢弃、好元素保留。
- `languages`：条目必须同时有 `id` 与 `schema`，否则整条丢弃；全丢光时回落到
  `default_languages()`。这条不是格式洁癖——`LanguageManager::load` 直接按下标取
  `current()`，留下一个用不了的条目比丢掉它更危险。
- 顶层不是 object（`[]`、裸字符串等）＝ 没有任何键可读，按「没有配置」处理，不算解析失败。
- `null` 视为「用户显式留空」，静默取缺省；类型不符才记 note。

被丢弃的键合并成**一条** note（`load_notes`，由 main() 打印）：列出被拒绝的键名。
一条而不是每键一条，是为了不淹没真正要紧的「failed to load」那条。

rime_*_data_dir 留空合法（由 rime 自行解析），**不做路径存在性检查**。
（`dict_path` / `extra_dicts` 已于 2026-09 之后删除，见 [[config-runtime-binding]]。）

`load()` 的两个"用默认值"分支（文件不存在、解析抛异常）统一
`return from_json(json::object())`，避免默认值在多处复制漂移。

测试：`tests/test_config.cpp` MaxCandidatesIsClampedAndTypeSafe /
LogLevelIsSanitized / CandidateBarPositionIsSanitized / **OneWrongTypedKeyDoesNotDiscardTheFile**
（后者含 2026-10-10 的实测复现体 `{"ui_language": 7, "shell": "/bin/zsh", "fuzzy_groups": ["zh_z","r"]}`，
并用修复前的 loader 验证过断言有区分度）。


## Timeline

- time: 2026-09-16T05:03:10
  kind: decision
  summary: "Created this page: 配置加载侧消毒：越界钳制、坏类型按缺省（不丢弃整份文件）"
  source: "2026-09-16 M1 落地"
  affects: [config-load-sanitization]

- time: 2026-09-28T14:20:29
  kind: decision
  summary: "load() 的两个“用默认值”分支（文件不存在、解析抛异常）统一改为 return from_json(json::object())，避免默认值在多处各自复制而漂移；解析语义（shell / 日志 / rime 目录如何接到运行时）另见 [[config-runtime-binding]]。"
  source: "2026-09-28 代码评审修复轮"
  affects: [config-load-sanitization]

- time: 2026-10-06T15:55:11
  kind: decision
  summary: "填实占位符：消毒原则 + 已落地范围 + 未覆盖字段"
  source: "2026-10-06 现状清理"
  affects: [config-load-sanitization]

- time: 2026-10-06T16:11:00
  kind: decision
  summary: "log_level/candidate_bar_position 补消毒，M1 枚举字段全覆盖"
  source: "2026-10-06 M1 推进"
  affects: [config-load-sanitization]

- time: 2026-10-09T08:02:01
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [config-load-sanitization]

- time: 2026-10-10T11:44:38
  kind: reversal
  summary: "评审复核（2026-10-10）：本页「坏类型不丢弃整份文件」的原则只对三个已消毒字段成立，其余字段仍在用会抛的 j.value()。实测 {\"ui_language\": 7, \"shell\": \"/bin/zsh\", \"fuzzy_groups\": [\"zh_z\",\"r\"]} 整体回退到默认：shell 丢成 $SHELL、fuzzy_groups 丢成 5 组。受害字段：shell / active_language / ui_language / fuzzy_groups（含 legacy fuzzy_pinyin 布尔键）/ rime_shared_data_dir / rime_user_data_dir / show_mode_indicator / log_file。根因是 nlohmann value() 在类型不匹配时抛 type_error.302，被 load() 的 catch 全量吞掉——修复面应是对这些键统一走 is_string()/is_array()/is_boolean() 预检，而不是给单个键打补丁。"
  source: "2026-10-10 全量代码评审复核"
  affects: [config-load-sanitization]

- time: 2026-10-10T16:44:31
  kind: decision
  summary: "消毒覆盖到全部键：坏类型按缺省并留一条 note，不再有任何 j.value() 抛出点"
  source: "2026-10-10 修复轮"
  affects: [config-load-sanitization]
