---
sidebar_position: 3
---

# 配置

配置文件位于 `~/.config/term-ime/config.json`。

## 完整配置

```json
{
  "languages": [
    {"id": "zh-Hans", "name": "简体中文", "enabled": true}
  ],
  "active_language": "zh-Hans",
  "max_candidates": 9,
  "fuzzy_pinyin": true,
}
```

## 配置项

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `languages` | array | `[{zh-Hans}]` | 可用语言列表 |
| `active_language` | string | `zh-Hans` | 当前激活的语言 |
| `max_candidates` | int | `9` | 每页候选词数量上限(1-9,越界自动钳制) |
| `fuzzy_pinyin` | bool | `true` | 模糊音开关(n/l、zh/z、r/l、r/y、hu/f、en-eng、an-ang) |
| `log_level` | string | `warn` | 日志级别: `debug`, `info`, `warn`, `error` |