---
id: librime-stl-contribution-policy
title: "librime-stl 维护策略：不主动维护，发现问题提 issue"
category: decision
status: active
tags: [librime, process, upstream]
created: "2026-10-09T08:50:56"
updated: "2026-10-09T08:51:13"
---

<!-- compiled_truth -->
**结论（2026-10-09 用户策略）**：`adam-ikari/librime-stl` 是 term-ime 的引擎依赖 fork，但 **term-ime 侧不再主动维护它**。

## 边界

- **不主动改 librime-stl**：不再为了 term-ime 的需求去改 fork 的源码、CMake、CI、dict 工具链。term-ime 只消费它（submodule 指针 + 引擎库 + `dict/` 词库）。
- **发现问题 → 提交 issue**：在 term-ime 使用中撞到 librime-stl 的缺陷或缺失，写成 issue 提到 `adam-ikari/librime-stl`，而不是就地改。
- **例外**：term-ime 自身的适配（如 submodule 指针更新、构建期数据同步、词库数据打包）仍留在 term-ime 侧，不动 fork。

## 与既有页面的关系

- [[dictionary-repo-split]]：词库已在 librime-stl 的 `dict/`，term-ime 侧不再持有任何词库维护物（本轮已落地）。
- [[librime-standalone-portability]] / [[engine-library-cross-platform]]：记录 fork 自身的缺陷与四平台产物 —— 这些缺陷今后以 issue 形式反馈。
- 本策略推翻「term-ime 顺手修 fork」的既有做法（如 2026-10-09 那批 fork 内提交）。


## Timeline

- time: 2026-10-09T08:50:56
  kind: decision
  summary: "Created this page: librime-stl 维护策略：不主动维护，发现问题提 issue"
  source: "用户策略 2026-10-09"
  affects: [librime-stl-contribution-policy]

- time: 2026-10-09T08:51:13
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [librime-stl-contribution-policy]
