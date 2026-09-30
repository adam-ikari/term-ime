---
id: termux-android-target
title: "Termux/Android 目标：交叉编译已支持，真机未验证"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-09-30T05:16:25"
---

<!-- compiled_truth -->
## 目标与现状（2026-09-30）

term-ime 的机制是 `forkpty()` + termios + `TIOCGWINSZ`/`SIGWINCH`，这套东西在
浏览器里一个都没有。所以「手机上能不能用」取决于 **Termux 提供的 PTY 是否满足
前提**，而不是取决于我们打包了什么——这一点决定了别用「能编译」去论证「能用」。

| | 状态 |
|---|---|
| 交叉编译出 arm64 Android 二进制 | ✅ NDK r27 验证通过，strip 后 3.1M |
| 真机 Termux 实测 | ⚠️ **未验证**（PTY/raw/软键盘下的 SIGWINCH 全是未知） |
| `install.sh` 一键装 | ❌ 只认 glibc 的 x86_64/aarch64 包 |

CI 的 `android-build` job 守住的是**交叉编译**，不是可用性。job 注释里写了这条
边界，别把它当成「支持手机」的验收。

## Android 与 glibc 的差异（全部 `if(ANDROID)` 隔离，宿主零回归）

宿主产物 BuildID 与加这些分支之前完全一致，仍全静态 —— 这是判断「有没有误伤
宿主构建」的硬判据，比跑测试更直接。

1. **不再 `-static`**。bionic 无可静态链接的 libc；`-static` 出来的二进制
   Android 加载器不认（仍要 `/system/bin/linker64` + 系统 libc）。卖点
   「单文件零依赖」在 Android 侧让位，改为只依赖 `libc/libm/libdl/liblog`。
2. **`forkpty` 在 libc 不在 libutil**。bionic 根本没有 libutil，硬链报
   `unable to find library -lutil`。同时要链 `log`（libuv Android 后端调
   `__android_log_print`）。
3. **vendored 依赖要转发 toolchain**。yaml-cpp/leveldb/marisa/opencc 是
   `execute_process` 起的**独立 cmake 工程**，不继承 toolchain，会悄悄按宿主
   架构编译，最后链接报 `is incompatible with aarch64linux`。
4. **NDK 的 find 路径要放开**。`CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY` 让
   `find_library` 看不到刚建好的 `_deps_stage`，报 `Could not find yaml-cpp
   library`，而那个 `.a` 就在旁边。改 `BOTH`。
5. **OpenCC 词典要用宿主工具生成**。opencc 在**构建期靠运行**自己的
   `opencc_dict` 生成 `.ocd2`；交叉编译后它是 Android 二进制，跑不了
   （`opencc_dict: not found` + `Error 127`）。词典与目标无关，故为宿主另编
   一份并**前置到 PATH**——`data/CMakeLists.txt` 里是普通 `set()` 写
   `OPENCC_DICT_BIN`，会遮蔽我们传的 `-D` 缓存变量，**改变量不生效，只能走 PATH**。

**API 28 是下限**：libuv 的 `process.c` 需要 `posix_spawn`，bionic 到 API 28
才有。低于 Android 9 编译直接失败。

## 下一步（按投入产出）

1. **真机验证**——没做之前，后续所有工作只是「能编译」。这是唯一的阻塞项。
2. **软键盘适配**——手机无物理键盘，候选栏靠方向键/数字键选词在触屏上不友好，
   可能需要长按或数字直选。这是产品问题不是构建问题。
3. **`install.sh` 支持 Termux**——需加 Android 分支 + `release.yml` 的
   `build-android` matrix 产物。

## 教训

CI 里给未验证的东西建 job 时，**要带负例自检**（拿已知不合格的输入跑同一段校验，
必须失败），否则断言可能是空转的。写 job 时直接用宿主二进制试过一遍，确认它
会红。


## Timeline

- time: 2026-09-30T05:15:49
  kind: decision
  summary: "Created this page: Termux/Android 目标：交叉编译已支持，真机未验证"
  source: created via brain create-page
  affects: [termux-android-target]

- time: 2026-09-30T05:16:25
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]
