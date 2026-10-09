---
id: engine-library-cross-platform
title: "librime-stl 引擎库跨平台产物：四目标 + 词库工具链解耦"
category: decision
status: active
tags: [librime, android, build, release]
created: "2026-10-09T05:22:01"
updated: "2026-10-09T05:22:26"
---

<!-- compiled_truth -->
## 引擎库是 librime-stl 的产物，四目标各出各的库

librime-stl（`adam-ikari/librime-stl`）的构建产物是**输入法引擎库**，不是 term-ime
二进制：`BUILD_SHARED_LIBS=ON` 出 `rime` 库（Linux `.so` / Windows `.dll` /
macOS `.dylib`），消费方（term-ime 或第三方）自行链接。词库是配套产物
（`dict/`，见 [[dict-artifact-publication]]），两者同仓但构建完全解耦。

四目标交付边界（2026-10-09 对齐）：

| 目标 | 形态 | 状态 |
|---|---|---|
| Linux | librime.so（静态链接 deps） | 已有 CI（linux-build.yml），term-ime 静态使用 |
| Windows | rime.dll（MSVC/clang x64/x86） | 已有 CI（windows-build.yml） |
| macOS | librime.dylib | 已有 CI（macos-build.yml） |
| Android | librime.so（NDK 交叉编译，纯 C API） | **新增**：只保证 NDK 能编出 .so，**不写 JNI 桥**（JNI 桥归消费方，如 Trime/fcitx5-android），验证深度 = 交叉编译通过、产物存在 |

## 词库工具链与引擎库解耦（已完成）

`dict/tools/`（Python + opencc 生成器）**永不进引擎编译图**：引擎 CMake 零引用
生成脚本，configure 期只对 `dict/opencc/*.json` 引用的表做只读闭包断言。工具链
独立验收是 `make -C dict/tools check`（复现门，diff 非空即脚本错）。交叉编译
（尤其 NDK）不要求 host Python / host opencc_dict —— opencc 的 `OPENCC_BUILD_DATA`
/`OPENCC_BUILD_TOOLS` 两开关（见 [[opencc-data-decoupling]]）保证这一点。

## 复现门是本轮修出来的（不是假设）

三个 bug 才让 `make check` 全绿：受保护字断言写反（占位符存活被误报为丢失）、
essay 网络抓取截断且缓存盲信（改 git 历史取源 `essay-base`）、rebuild_ocd2.sh
用 `$0` 相对路径算 dict 目录（裸文件名时落到 `$PWD`）。全部提交在
librime-stl `a0b59372`。


## Timeline

- time: 2026-10-09T05:22:01
  kind: decision
  summary: "Created this page: librime-stl 引擎库跨平台产物：四目标 + 词库工具链解耦"
  source: created via brain create-page
  affects: [engine-library-cross-platform]

- time: 2026-10-09T05:22:26
  kind: decision
  summary: "需求重整理后定界"
  source: "2026-10-09 需求对齐"
  affects: [engine-library-cross-platform]
