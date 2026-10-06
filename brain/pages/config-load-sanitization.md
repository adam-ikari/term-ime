---
id: config-load-sanitization
title: "配置加载侧消毒：越界钳制、坏类型按缺省（不丢弃整份文件）"
category: decision
status: active
tags: [config, robustness, rime]
created: "2026-09-16T05:03:10"
updated: "2026-10-06T15:55:11"
---

<!-- compiled_truth -->
配置加载侧消毒原则：越界钳制、坏类型按缺省（不丢弃整份文件）。

落地范围（`src/core/config.cpp` `from_json`）：
- **`max_candidates`**：钳制到 [1,9]（单数字选择键 "123456789" 的合法范围）；
  非整数（如字符串）按缺省 9，兼容旧键 `page_size`。越界或坏类型不会让整份配置失效。
- 其余字段（shell / languages / active_language / ui_language / dict_path /
  extra_dicts / fuzzy_groups / rime_*_data_dir / show_mode_indicator /
  candidate_bar_position / log_level / log_file）目前仅做"存在则取、缺则默认"，
  未做枚举校验或路径存在性检查。

`load()` 的两个"用默认值"分支（文件不存在、解析抛异常）统一
`return from_json(json::object())`，避免默认值在多处复制漂移。

M1 完成定义未达成：`log_level`、`candidate_bar_position` 等字段待补消毒。


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
