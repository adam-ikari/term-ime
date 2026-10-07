---
id: release-binary-strip
title: "release 二进制默认 strip（5.4M→4.2M）"
category: decision
status: active
tags: [build, size, release]
created: "2026-10-07T09:12:50"
updated: "2026-10-07T09:12:50"
---

<!-- compiled_truth -->
发布二进制**默认 strip**，实测 **5.4M → 4.2M（-22%）**。

## 为什么

CMakeLists 的优化注释里早就写了「stripped ~4.9M」，但构建链里**从来没有
strip 这一步**——注释描述的是一个从未实现的预期。加上 `STRIP_RELEASE` 选项
（默认 ON，Debug 构建跳过）后才真正达到，而且实测比注释预期更低（4.2M）。

## 为什么放心 strip

崩溃报告落在**发布的二进制**上，不是符号化产物。term-ime 没有 core dump 符号化
流程，也不在 release 里带 `.debug` 文件，所以符号表对任何实际用途都是纯负担。
需要调试 release 构建时用 `-DSTRIP_RELEASE=OFF`。

## 数据

|项|值|
|---|---|
|strip 前|5.4M|
|strip 后|4.2M|
|ctest|120/120 通过|

体积优化的其余方向已由 [[dictionary-repo-split]] 记录为不可行：essay.txt 是候选
主力源（不只是赋权），裁剪会删掉常用词候选。


## Timeline

- time: 2026-10-07T09:12:50
  kind: decision
  summary: "Created this page: release 二进制默认 strip（5.4M→4.2M）"
  source: "2026-10-07 strip 优化"
  affects: [release-binary-strip]

- time: 2026-10-07T09:12:50
  kind: decision
  summary: "release 产物默认 strip；此前 CMake 注释预期 4.9M 但从未实现"
  source: "2026-10-07 实测优化"
  affects: [release-binary-strip]
