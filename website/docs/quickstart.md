---
sidebar_position: 1
---

# 快速开始

## 用户安装（推荐，无需 sudo）

```bash
# 一键安装到 ~/.local/bin，完全静态，零依赖
curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
```

## 系统安装（需要 sudo）

```bash
# 安装到 /usr/local/bin，所有用户可用
curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash -s -- --prefix /usr/local
```

## 从源码构建

```bash
git clone --recursive https://github.com/adam-ikari/term-ime.git
cd term-ime
make build
./build/term-ime
```

> 命令名：安装脚本会把程序装成短命令 `ti`（terminal input），并保留 `term-ime` 软链接别名。从源码构建时直接运行 `./build/term-ime`，或把它放到 PATH 后用 `ti` 调用。

:::tip 字体推荐
推荐在终端使用等宽字体（如 [Maple Mono](https://github.com/subframe7536/maple-font)、Sarasa Mono、JetBrains Mono），以获得最佳的中文与候选词对齐效果。
:::

## 配置（可选）

配置文件位于 `~/.config/term-ime/config.json`。可在这里开关语言、切换界面语言。

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

:::caution
需在真实 TTY 或支持 alternate screen 的终端中运行。
:::