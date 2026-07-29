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
  "log_level": "warn"
}
```

## 配置项

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `languages` | array | `[{zh-Hans}]` | 可用语言列表 |
| `active_language` | string | `zh-Hans` | 当前激活的语言 |
| `ui_language` | string | `zh-CN` | 界面显示语言 |
| `log_level` | string | `warn` | 日志级别: `debug`, `info`, `warn`, `error` |