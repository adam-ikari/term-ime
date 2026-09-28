---
id: config-load-sanitization
title: "配置加载侧消毒：越界钳制、坏类型按缺省（不丢弃整份文件）"
category: decision
status: active
tags: [config, robustness, rime]
created: "2026-09-16T05:03:10"
updated: "2026-09-28T14:20:29"
---

<!-- compiled_truth -->
<current best understanding — replace this with the real content>

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
