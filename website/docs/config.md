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
  "ui_language": "zh-CN",
  "max_candidates": 9,
  "fuzzy_groups": ["zh_z", "n_l", "r", "hu_f", "nose"],
  "log_level": "warn"
}
```

## 配置项

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `languages` | array | `[{zh-Hans}]` | 可用语言列表 |
| `active_language` | string | `zh-Hans` | 当前激活的语言 |
| `ui_language` | string | `zh-CN` | 界面显示语言 |
| `max_candidates` | int | `9` | 每页候选词数量上限(1-9,越界自动钳制) |
| `fuzzy_groups` | array | `["zh_z","n_l","r","hu_f","nose"]` | 模糊音开关（每组独立）：`zh_z` 平翘舌、`n_l` n/l、`r` r 系、`hu_f` h/f、`nose` 前后鼻音；空数组 = 精确拼音。旧配置 `fuzzy_pinyin`（bool）仍兼容读取 |
| `log_level` | string | `warn` | 日志级别: `debug`, `info`, `warn`, `error` |