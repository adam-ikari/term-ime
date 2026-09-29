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
| `shell` | string | `$SHELL`，取不到则 `/bin/bash` | 启动时在最外层 PTY 里跑的 shell |
| `rime_shared_data_dir` | string | 空 | 拼音数据目录。留空自动查找（离二进制最近的 `share/term-ime/rime-data` 优先，见更新日志 v1.1.4）；填了就直接用，不再搜索。数据在非常规位置、或机器上另有系统 `/usr/share/rime-data` 想要强行指定自己那份时才需要填 |
| `log_level` | string | `warn` | 日志级别: `debug`, `info`, `warn`, `error` |
| `log_file` | string | 空 | 日志文件路径。留空写到 `~/.cache/term-ime/term-ime.log` |

## 日志

排查问题先把 `log_level` 调到 `debug`，日志默认在 `~/.cache/term-ime/term-ime.log`，`log_file` 可以换到别处。

warn 及以上立即落盘，其余最多缓一秒。进程被信号打断、或者终端被直接关掉时，崩溃前那几行仍在文件里。

`log_file` 指的路径写不进去（目录建不出来、没权限）时，日志不会被关闭：程序继续写默认位置，并在里面留下一句"仍在写到哪个文件"。配置文件本身解析失败时，这条诊断也写在你配置的那个文件里，不是启动日志。