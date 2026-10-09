---
id: opencc-data-decoupling
title: "librime↔libopencc 边界：引擎只链接，简繁数据作为词库产物"
category: decision
status: active
tags: [rime, opencc, build, portability]
created: "2026-10-09T03:15:51"
updated: "2026-10-09T03:16:20"
---

<!-- compiled_truth -->
## 边界：引擎链接 libopencc，数据来自词库产物

librime 只用 opencc 两件事：**链接** `libopencc`，以及在运行时按名字读一份
opencc 配置。读路径是 `simplifier.cc` 把 `opencc_config` 解析成
`<shared_data_dir>/opencc/<config>` —— **不是** opencc 自己的安装前缀。所以
「opencc 装机时生成的那份 share/opencc」对引擎运行毫无作用。

由此定下的边界（2026-10-09）：

- **opencc 的词库生成不属于引擎编译图。** opencc 上游在 `data/CMakeLists.txt`
  configure 期 `find_package(PythonInterp REQUIRED)`，并把 `Dictionaries` 做成
  `ALL` 目标、在 build 期**执行**现编译的 host `opencc_dict`。这两条挂在引擎
  构建上，Windows 就要求 host Python，NDK 交叉编译直接 `Exec format error`
  （被执行的是交叉编译产物）。
- 修法只加两个开关，默认 `ON` 以保持上游行为：`OPENCC_BUILD_DATA`（罩住
  `add_subdirectory(data)`）、`OPENCC_BUILD_TOOLS`（罩住 `src/CMakeLists.txt` 的
  `add_subdirectory(tools)`）。生成依赖 tools，所以配置期断言
  `OPENCC_BUILD_DATA AND NOT OPENCC_BUILD_TOOLS` 必须报错。消费方
  （fork 的 `deps.mk`/`build.bat`、term-ime 的 `CMakeLists.txt`）两个都传 `OFF`。
- **简繁数据是词库产物的一部分**，提交在 `deps/librime/dict/opencc/`，并要求
  自足：配置里引用的每个 `.ocd2`/`.txt` 都得同目录躺着。fork 的 `CMakeLists.txt`
  在 configure 期遍历 `dict/opencc/*.json` 的 `"file"` 键做闭包断言，
  term-ime 的 `test_rime_data.cpp` 再从 schema 的 `opencc_config:` 反向扫一遍。

## PKGDATADIR 不是运行时输入（曾经的静默失效）

`t2hk.json`/`t2tw.json` 与 `HK`/`TWVariants.ocd2` 此前**只**存在于 opencc 的安装
前缀，靠烧进 `libopencc.a` 的绝对 `PKGDATADIR` 被 `Config.cpp` 的兜底路径找到
（`strings libopencc.a` 实测打印出构建机路径）。离开构建机，`simplifier.cc`
catch 之后返回 `nullptr`，filter 静默消失 —— 不报错、不降级，只是港/臺切换
没有效果。补齐这 4 个文件（约 10 KB）之后，`PKGDATADIR` 里还编在库里但已
不被依赖；判据不是「它不存在」，而是「构建机那份数据目录整个不存在时，
闭包测试仍绿」。

## opencc 是 vendored，不是第三个 fork

`deps/librime/deps/opencc` 原来是指向 `BYVoid/OpenCC` 的 submodule（pin
`556ed224`，ver.1.1.9）。现改成 vendored 进仓：打补丁要给第三方仓建 fork，
第三个 fork 只会让「同步上游」变成三方合并。现在补丁只有上面那两处构建开关，
全部记在 `deps/opencc/UPSTREAM.md`（pin SHA + 补丁清单 + 重新同步步骤），
同步上游 = 按 SHA 取档 + 重放补丁。submodules 6→5。

坑：fork 的 `.gitignore` 有一条 `deps/*/*`，会把 vendored 源码整个吞掉 ——
`git add deps/opencc` 静默只暂存 2 个文件。vendored 依赖必须显式开
`!deps/opencc/**` 例外，同时继续忽略它的 `build/`。

## 词库生成工具独立于 CMake

`dict/tools/`（`make -C dict/tools check`）是这条边界的另一侧：生成器
（host Python + 现编译的 `opencc_dict`）只有这里能用，故意不接进 CMake ——
接进去哪怕做成非 `ALL` 目标，也会把「配置引擎」和「生成词库」绑回同一张
编译图。详见 [[dict-artifact-publication]] 与 [[dictionary-repo-split]]。


## Timeline

- time: 2026-10-09T03:15:51
  kind: decision
  summary: "Created this page: librime↔libopencc 边界：引擎只链接，简繁数据作为词库产物"
  source: "2026-10-09 词库与引擎编译目标拆分"
  affects: [opencc-data-decoupling]

- time: 2026-10-09T03:16:20
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-10-09 vendored opencc + 词库数据闭包"
  affects: [opencc-data-decoupling]
