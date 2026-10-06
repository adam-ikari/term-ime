---
id: cpp-standard-floor
title: "librime C++ 标准地板=17：不可降级到 C++11"
category: decision
status: active
tags: [librime, build, cpp-standard, constraint]
created: "2026-10-06T15:40:14"
updated: "2026-10-06T15:40:25"
---

<!-- compiled_truth -->
librime fork（`v1.1.8-rime-stack`）以 C++17 为地板，不可降级到 C++11。

致命障碍是 `std::filesystem`：`src/rime/common.h:82` 定义 `class path : public
std::filesystem::path`（librime 全树路径抽象），78 个文件经 `common.h` / `rime::path`
间接依赖。`std::filesystem` 是 C++17 才进入标准的——降 C++11 必须重写整个路径层，
替代只能引回 `boost::filesystem`，与 fork 已完成的 drop-Boost 方向冲突。

其余 C++17 用法可单独替换，不构成降级依据：
- `std::any`：`deployer.h:22` `TaskInitializer = std::any`（可换 boost::any / void*）
- `std::make_unique`（C++14）：7 处（可手写 shim）
- `std::string_view`：仅 `include/utf8/cpp17.h`（utfcpp 可选 C++17 API），librime 自身
  未调用，可裁掉该 include

结论：C++17→C++11 不是"改标准"，是 78 文件级的路径层重构，否决。fork 的裁剪
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
