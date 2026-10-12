---
id: cpp-standard-floor
title: "librime C++ 标准地板=17：不可降级到 C++11"
category: decision
status: active
tags: [librime, build, cpp-standard, constraint]
created: "2026-10-06T15:40:14"
updated: "2026-10-12T06:44:07"
---

<!-- compiled_truth -->
librime fork 以 C++17 为地板，不可降级到 C++11。

**2026-10-12 重验（pin `f0dd67a8`，82 提交含上游 67 提交合并之后）**：地板结论不变
——fork `CMakeLists.txt:6` 仍是 `set(CMAKE_CXX_STANDARD 17)`、`CMakeLists.txt:219` 仍是
`-std=c++17`，本仓根 `CMakeLists.txt:4` 同为 17。**判据不再钉 fork tag**：tag 只是快照
（`v1.1.8-rime-stack` 已被 `v1.1.9-rime-stack` 取代，而 pin 永远在 tag 之后游走），本页
只认「fork 当前 pin」这个坐标。

致命障碍是 `std::filesystem`：`src/rime/common.h:82` 定义 `class path : public
std::filesystem::path`（librime 全树路径抽象），原计 78 个文件经 `common.h` /
`rime::path` 间接依赖；2026-10-12 直接 include `rime/common.h` 的已有 125 个文件、直接
用 `std::filesystem` 的 13 个，**只增不减**。降 C++11 必须重写整个路径层，替代只能引回
`boost::filesystem`，与 fork 已完成的 drop-Boost 方向冲突。

其余 C++17 用法可单独替换，不构成降级依据：
- `std::any`：`deployer.h:22` `TaskInitializer = std::any`（可换 boost::any / void*）
- `std::make_unique`（C++14）：1 处（可手写 shim；2026-10-06 时 7 处，上游合并后缩到 1）
- `std::string_view`：**2026-10-12 推翻原断言「librime 自身未调用，可裁掉 utf8/cpp17.h
  的 include」**——上游把 `std::string_view` 直接写进了 librime 自己的代码
  （`syllabifier.cc:69`、`prism.h:83-86`、`prism.cc:263`，各自 `#include <string_view>`，
  来自 `perf(prism): GetValue accepts string_view` 一系提交）。它不再是 utfcpp 的可选
  API，而是 librime 的一等公民；这条反而把地板钉得更死（C++11 连替换方案都没有）。

结论：C++17→C++11 不是"改标准"，是全树路径层重构，否决。fork 的裁剪
（drop Boost、std::regex 替换 boost::regex）已是合理终点，无进一步降级空间。

依赖侧维持现状：yaml-cpp / leveldb / marisa-trie / opencc 四个 vendored 依赖是
功能必需（schema 解析 / 用户词典 / trie / 简繁转换），去掉等于失去输入法能力；
"零运行时依赖"（静态单文件二进制）和"零外部系统依赖"（全 vendored）均已成立。


## Timeline

- time: 2026-10-06T15:40:14
  kind: decision
  summary: "Created this page: librime C++ 标准地板=17：不可降级到 C++11"
  source: "2026-10-06 降级可行性评估"
  affects: [cpp-standard-floor]

- time: 2026-10-06T15:40:25
  kind: decision
  summary: "librime fork 不可降级到 C++11；std::filesystem 是硬地板"
  source: "2026-10-06 评估后否决降级"
  affects: [cpp-standard-floor]

- time: 2026-10-12T06:44:07
  kind: decision
  summary: "pin 升到 f0dd67a8 后重验：地板仍是 17；判据改认 pin 不认 fork tag；string_view 一节按上游新代码改写"
  source: "2026-10-12 升级 librime 子模块"
  affects: [cpp-standard-floor]

- time: 2026-10-12T06:44:07
  kind: reversal
  summary: "推翻「librime 自身未调用 string_view，可裁掉 utf8/cpp17.h 的 include」：上游把 std::string_view 直接写进 syllabifier.cc:69 / prism.h:83-86 / prism.cc:263"
  source: "上游 67 提交合并，perf(prism): GetValue accepts string_view"
  affects: [cpp-standard-floor]
