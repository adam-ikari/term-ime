---
id: startup-readiness-window
title: "冷启动就绪窗口：rime 首次部署期间应用不接受输入"
category: decision
status: active
tags: [rime, startup, pty, testing]
created: "2026-09-15T03:17:45"
updated: "2026-09-15T03:17:45"
---

<!-- compiled_truth -->
## 已观测事实

- 冷启动（全新 `XDG_DATA_HOME`）：librime 首次部署（prism/table 编译）阻塞 `App::init` 约 **5.0s**；该窗口内终端只有 25 字节输出（进 alt screen + 隐藏光标）＝白屏。
- 热启动（`XDG_DATA_HOME` 已部署）：0.6s 内可接受输入。
- 键盘 watch 在 `App::init()` 返回之后才注册（`main.cpp`），所以窗口内的按键**全部丢失**，不是渲染缺陷。

## 约束

- `App::init` 必须在进入 rime 初始化这一慢路径**之前**画出 `status.initializing` 提示帧，并在部署完成后擦除同一行；用 `Renderer::forward_output` 裸写，不写进 Screen 模型（避免 shell 提示符覆盖后残留半个字形）。
- 任何 PTY 端到端测试/脚本在断言前必须做**就绪门**（有界重试直到目标标记出现），否则会把启动窗口读成「整帧空白/半帧」。

## 反例（结论）

200 次开/关设置面板压力：读侧「未收敛即读」与「收敛后才读」两种策略均 **0/200 坏帧** → 面板渲染不存在缺陷；此前观察到的「1/11 空白帧」是启动就绪窗口 + 调用者 `$SHELL` 造成的测量假象。


## Timeline

- time: 2026-09-15T03:17:45
  kind: decision
  summary: "Created this page: 冷启动就绪窗口：rime 首次部署期间应用不接受输入"
  source: "2026-09-15 白屏/半帧缺陷定位"
  affects: [startup-readiness-window]

- time: 2026-09-15T03:17:45
  kind: decision
  summary: "冷启动（全新 XDG_DATA_HOME）时 librime 首次部署在 App::init 内阻塞约 5s，期间键盘输入不被处理；必须先画提示帧，测试必须先等就绪门。"
  source: "2026-09-15 白屏/半帧缺陷定位"
  affects: [startup-readiness-window]
