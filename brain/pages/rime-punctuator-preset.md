---
id: rime-punctuator-preset
title: "标点映射必须由 schema 显式 import_preset（librime 不隐式继承 punctuator）"
category: decision
status: active
tags: [rime, 标点, punctuator, schema]
created: "2026-09-24T10:34:16"
updated: "2026-09-24T10:34:23"
---

<!-- compiled_truth -->
**结论**：schema 想用标点映射，必须在自己的 `.schema.yaml` 里写 `punctuator: { import_preset: default }`。librime **不会**隐式把 default.yaml 的 `punctuator` 段合并进 schema——只有 `guarded_by` 的 `menu`/`navigator`/`selector`（DefaultConfigPlugin）是自动合并的，`punctuator` 靠 `LegacyPresetConfigPlugin` 读 schema 自己的 `punctuator/import_preset`（或 `recognizer/import_preset`、`key_binder/import_preset`）才解析。

**症状**：中文模式下按 `,`/`?`/`!` 等不出全角标点，键被 librime `process_key` 直接拒绝（`Punctuator::ProcessKey` 在 `mapping_`/`symbols_` 都空时返回 false），选字面板也不出现。

**根因现场**：`luna_pinyin.schema.yaml` 有 `punctuator: import_preset: symbols`；term-ime 自带的 `luna_pinyin_simp.schema.yaml` 与 `luna_pinyin_simp_fuzzy.schema.yaml` 漏了这一段，编译后的 schema 里没有 `punctuator/half_shape`。

**修复**：两份 simp schema 各加 `punctuator: { import_preset: default }`（default.yaml 里 full_shape/half_shape 两张表齐全）。模糊音子集 schema 由模板裁剪生成，因此自动继承。

**约束**：任何新增/派生 schema 若要用标点，必须自带 `punctuator` 段；不要指望上游 `import` 链替它补齐。验证方式见 `tests/test_punctuation.py`（全角 6 键 + ASCII 透传 + 产物 schema 含 `punctuator:`）。


## Timeline

- time: 2026-09-24T10:34:16
  kind: decision
  summary: "Created this page: 标点映射必须由 schema 显式 import_preset（librime 不隐式继承 punctuator）"
  source: "本次会话：中文模式逗号不转全角（PunctFix）"
  affects: [rime-punctuator-preset]

- time: 2026-09-24T10:34:23
  kind: decision
  summary: "记录根因与契约"
  source: "本次会话 PunctFix"
  affects: [rime-punctuator-preset]
