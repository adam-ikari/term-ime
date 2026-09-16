---
id: config-save-never-throws
title: "AppConfig::save 永不抛异常、返回 bool"
category: decision
status: active
tags: [config, error-handling, libuv]
created: "2026-09-14T15:18:27"
updated: "2026-09-14T15:18:27"
---

<!-- compiled_truth -->
`AppConfig::save` **不抛异常**，返回 `bool` 表示成功与否（写失败、目录不可写、序列化失败都走 `false`）。

## 原因

它从 **libuv 回调**（事件循环、文件/信号回调）里被调用。libuv 回调是 C 栈帧，异常穿过它没有 handler，结果是 `std::terminate`——进程直接死，没有栈展开、没有日志，且发生时机是"用户改设置"这类非致命路径。

## 规则

1. `save` 内部 `try`/`catch(...)` 兜底，失败返回 `false`；调用方检查返回值并决定提示，不依赖异常。
2. 日志/报错只能走返回码 + 日志输出，不要借用异常传播。
3. 新增"从回调里调用"的持久化入口沿用同一契约。

## 影响面

`src/core/config.{hpp,cpp}`、`src/ui/settings.cpp` 的保存路径。相关：[[event-loop-libuv-handle-ownership]]（同一类"回调栈帧里不能逃逸"的约束）。


## Timeline

- time: 2026-09-14T15:18:27
  kind: decision
  summary: "Created this page: AppConfig::save 永不抛异常、返回 bool"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [config-save-never-throws]

- time: 2026-09-14T15:18:27
  kind: decision
  summary: "AppConfig::save 签名是 noexcept 语义：捕获一切失败返回 bool，绝不抛异常——它从 libuv 回调里被调用，异常逃逸会直接 std::terminate"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [config-save-never-throws]
