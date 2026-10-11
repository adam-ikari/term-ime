---
id: install-target-incomplete
title: "install 目标不完整：二进制没装、头文件集不可用、3 个测试二进制未注册 CTest"
category: decision
status: active
tags: [cmake, install, packaging, ci]
created: "2026-10-10T11:45:21"
updated: "2026-10-10T17:33:17"
---

<!-- compiled_truth -->
`cmake --install` 交付的是**应用程序本身**，不是内部静态库。2026-10-10 一轮把三处不一致都收掉了。

## 安装树内容（现在的口径）

- `bin/term-ime`（`install(TARGETS term-ime RUNTIME DESTINATION bin)`）
- `share/term-ime/translations`、`share/term-ime/rime-data`、`LICENSE`、`LICENSES/`
- **不再**安装 `term-ime-lib` / `term-terminal` / `term-core` 三个归档，也不再安装那 5 个头文件。

删掉库/头的理由不是「懒得补」，而是：本项目真正对外交付的引擎库是 **librime-stl 的产物**
（见 [[engine-library-cross-platform]]），这几个 .a 是内部切分；而原来那份头文件集合
自身不闭合（`rime_engine.hpp` 依赖未安装的 `rime_data.hpp` 与 `<rime_api.h>`，
`language.hpp` 依赖未安装的 `core/config.hpp`），rime-static/ftxui/uv_a 的依赖也一个没装。
「装出去但既编不过也链不上」比不装更坏。

## 已验证（2026-10-10，安装到 /tmp/inst 后在 PTY 里实测）

日志实据：`Found translations path: <install>/bin/../share/term-ime/translations`、
`rime shared data dir: <install>/bin/../share/term-ime/rime-data`；输入 nihao 出 8 个候选、
按 `1` 提交「你好」。即安装树**不依赖 build 树**可用——运行时的查找顺序把
`<exe_dir>/../share/term-ime/*` 排在编译期内建的 `RIME_BUNDLED_DATA_DIR` 之前
（`src/ime/rime_engine.cpp`、`src/util/i18n.cpp`），所以这条链是真的走通的。

## 测试注册

`test-ui-jsx` / `test-settings` / `test-input-e2e` 是自带 `main()`、失败返回非 0 的手工
程序，`gtest_discover_tests` 对它们无效，因此用 `add_test(NAME ... COMMAND ...)` 注册。
`ctest -N` 从 124 项变成 135 项。ci.yml 里那段手工逐个跑三个二进制的步骤同时删掉——
同一个用例不该有两条执行路径（本地 ctest 曾长期静默漏掉它们，而 CI 是绿的）。

## 发布闸门（原先「仍未处理」的那条，已处理）

release.yml 新增 `tests` job：checkout（含 submodules）→ 与 `build-linux` 同一 Ubuntu
版本、同一套 `-static` 链接参数构建 → `ctest --output-on-failure` → `./tools/run-e2e.sh`；
`release` 改为 `needs: [build-linux, tests]`。此前 release.yml 一个测试都不跑就以
`draft: false` 发布，而 install.sh 默认指向 `/releases/latest`——「没测」和「发给所有人」
是同一个事件。现在这两者之间有了闸门。

两条已知代价写在 workflow 注释里：其一，job 之间不能共享对象文件，多出一次构建；
其二只覆盖 x86_64，aarch64 产物仍只受 `build-linux` 的编译 + 零动态依赖检查约束。

e2e 清单抽到 `tools/run-e2e.sh`，ci.yml 与 release.yml 共用同一份——之前它内联在
ci.yml 里，正是 release 能绕开全部 e2e 的结构原因。


## Timeline

- time: 2026-10-10T11:45:21
  kind: decision
  summary: "Created this page: install 目标不完整：二进制没装、头文件集不可用、3 个测试二进制未注册 CTest"
  source: created via brain create-page
  affects: [install-target-incomplete]

- time: 2026-10-10T11:45:33
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [install-target-incomplete]

- time: 2026-10-10T16:57:58
  kind: decision
  summary: "install 树改成只交付可执行文件+数据+许可；三个手工测试二进制已注册 CTest"
  source: "2026-10-10 修复轮"
  affects: [install-target-incomplete]

- time: 2026-10-10T17:33:17
  kind: decision
  summary: "install 树只交付可执行文件+数据+许可；三个手工测试二进制注册进 CTest；release.yml 增加 tests job 作为发布闸门"
  source: "2026-10-10 修复轮（release 闸门补齐）"
  affects: [install-target-incomplete]
