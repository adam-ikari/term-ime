---
id: rime-data-dir-discovery
title: "rime 共享数据目录解析：可执行文件相对优先 + 以自带 schema 为标记"
category: decision
status: active
tags: [rime, packaging, install, release]
created: "2026-09-29T02:06:30"
updated: "2026-10-09T06:21:43"
---

<!-- compiled_truth -->
**结论（v1.1.4 起）**：`RimeIme::initialize()` 的 shared_data_dir 候选顺序，第一个"带标记"的目录胜出，与列表顺序无关；一个标记都没有时回落为第一个存在的目录（`select_shared_data_dir`，`src/ime/rime_data.hpp/.cpp`）：

1. `<exe_dir>/../share/term-ime/rime-data` —— tarball、Homebrew Cellar（`prefix.install "term-ime/share"` 让 share 落在 Cellar 版本目录内，故 `bin/../share` 命中）、install.sh 的 `PREFIX` 三家同一个形状
2. `<exe_dir>/share/rime-data`
3. `RIME_BUNDLED_DATA_DIR`（编译期烧进去的**构建机绝对路径**，绝不能当成运行时唯一线索）
4. `~/.local/share/term-ime/rime-data`、`/usr/local/share/term-ime/rime-data`、`/usr/share/term-ime/rime-data`
5. `/usr/share/rime-data`、`/usr/local/share/rime-data`（系统 librime 数据，排最后）

`AppConfig::rime_shared_data_dir` 非空时直接胜出（用户显式选择，marker 与否都不覆盖）。

**"是不是 term-ime 自带的数据"的判据是文件标记，不是目录存在**：`kRimeDataMarker = "luna_pinyin_simp_fuzzy.schema.yaml"`。系统 `/usr/share/rime-data` 满足 `exists()` 但没有这份方案，选中它的症状是 `[拼]` 正常亮起、切换正常、打字零候选 —— 静默哑掉，用户无从下手（v1.1.3 就是这么发布出去的）。无论最终目录来自搜索还是配置，缺标记都 `spdlog::warn` 写明"缺哪份方案、拼音不会可用"，另有一行 `info` 记录选中的目录。

**约束**：
- 选择逻辑抽在 `rime_data.hpp`（不含 librime 类型）。原因：`deps/librime/src/rime_api.h` 的 `#define Bool int` 与 gtest 的 `internal::ParamGenerator<bool> Bool()` 冲突，凡 include `rime_engine.hpp` 的测试都编不过。测试只 include `rime_data.hpp`。
- 发布产物必须把 rime-data 放到 `share/term-ime/rime-data/`（`release.yml` 第 58/63 行、CMake install、install.sh 同构），否则 exe 相对候选落空。
- 任何改这条顺序的动作都要跑 `tests/test_rime_data.cpp`（5 条：标记优先含顺序颠倒、显式配置优先、无标记仍可启动、全不存在回退、文件不当目录）。

**验证手法（可复用）**：`unshare -rm` + `mount --bind /tmp/empty-rime <build>/share/rime-data` 隐藏编译期路径，再用 `mount --bind /tmp/decoy-rime /usr/share/rime-data` 放诱饵；pty 脚本用干净 HOME 跑 `nihao` 看候选。老二进制在同场景 `candidates=False`，即负对照。


## Timeline

- time: 2026-09-29T02:06:30
  kind: decision
  summary: "Created this page: rime 共享数据目录解析：可执行文件相对优先 + 以自带 schema 为标记"
  source: created via brain create-page
  affects: [rime-data-dir-discovery]

- time: 2026-09-29T02:06:52
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "v1.1.4 数据目录定位修复 + unshare A/B 验证"
  affects: [rime-data-dir-discovery]

- time: 2026-10-09T06:21:43
  kind: decision
  summary: "词库数据改为构建期同步（CMakeLists rime-data ALL 目标 copy_if_different），并把 deps/librime/dict/VERSION 一并打包。原因：dict/ 在 submodule 里，submodule 更新词库不触碰本仓任何 tracked 文件，configure_file 会一直供旧字节直到有人重跑 cmake —— 静默陈旧。VERSION 让部署树自描述词库坐标。数据字节不变，目录解析契约（marker luna_pinyin_simp_fuzzy.schema.yaml）不变"
  source: term-ime
  affects: [rime-data-dir-discovery]
