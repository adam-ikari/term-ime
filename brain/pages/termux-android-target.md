---
id: termux-android-target
title: "Termux/Android 目标：交叉编译已支持，真机未验证"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-10-01T06:07:13"
---

<!-- compiled_truth -->
## 目标与现状（2026-10-01 更新：已在模拟器实测跑通）

term-ime 的机制是 `forkpty()` + termios + `TIOCGWINSZ`/`SIGWINCH`。在 Android
API 31 模拟器（x86_64 ABI）上实测：**forkpty、librime 全流程、per-combination
生成 schema、候选栏渲染、模糊音，全部工作正常**，打出 `nihao` → `1.你好
2.利好 3.立好 4.理好 5.立号`，与 Linux 一致；`la` → `1.那`（n/l 模糊音生效）。

仍未知的是**真机**（非模拟器）特有部分：软键盘弹起/收起时的 `SIGWINCH`、
状态栏是否被输入法窗口挤压、长按选词交互。这些模拟器测不出来。

| 项目 | 状态 |
|---|---|
| 交叉编译 arm64 Android | ✅ NDK r27，strip 后 3.1M |
| **在 Android 用户空间实跑** | ✅ **模拟器实测通过**（x86_64 ABI，API 31） |
| 真机（软键盘/resize） | ⚠️ 未验证，需真机 |
| `install.sh` 安装 | ✅ 同一条命令，脚本自动识别平台 |

CI 的 `android-build` 只验交叉编译；实跑验证靠模拟器（`ANDROID_ABI=x86_64`
交叉编译一份再 `adb push`，用 `forkpty` 包一层提供 PTY 注入按键）。

## Android 与 glibc 的差异（全部 `if(ANDROID)` 隔离，宿主零回归）

宿主产物 BuildID 与加这些分支之前完全一致，仍全静态 —— 这是判断「有没有误伤
宿主构建」的硬判据，比跑测试更直接。

1. **不再 `-static`**。bionic 无可静态链接的 libc；`-static` 出来的二进制
   Android 加载器不认。改为只依赖 `libc/libm/libdl/liblog`。
2. **`forkpty` 在 libc 不在 libutil**。bionic 根本没有 libutil，硬链报
   `unable to find library -lutil`。同时要链 `log`。
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
才有。实测 API 31 模拟器无问题。

## 模拟器实测方法（可复用）

```bash
# 1. 为 x86_64 交叉编译（模拟器只有这个 ABI）
cmake -B build-and -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-28 -DCMAKE_BUILD_TYPE=Release
cmake --build build-and --target term-ime
# 2. 起模拟器（本机无 KVM，需 -accel off，软件模拟慢：词典部署约 5 秒）
emulator -avd <name> -no-window -no-audio -no-boot-anim -gpu swiftshader_indirect -accel off
# 3. push 二进制 + share/，用 forkpty 包一层提供 PTY 注入按键
```

**坑**：无 TTY 时 term-ime 会在 renderer 初始化处优雅退出（`Not a TTY`），
词典根本没机会部署 —— 所以验证必须给 PTY。`drain()` 若用局部 `total`
累加，多次调用只会保留最后一次的输出（我踩过，表现为「捕获 290 字节」）。

## 下一步

1. **真机验证**——模拟器测不出软键盘 resize 与输入法窗口遮挡。
2. **软键盘适配**——无物理键盘时方向键选词不友好，需长按/数字直选。
3. **`install.sh` 的 Termux tag 常量需随发版更新**（见下）。

## install.sh 的坑（发版时要记得改）

Termux 版本解析用**已知 tag 常量**而非 API 探测：`VERSION="${TERM_IME_TERMUX_TAG:-v1.1.7-termux}"`。
原因是 API 探测每次安装要 1 次 API + 最多 30 次 HEAD，而未认证限额仅 60 次/小时/IP，
几个人同时装就耗尽，之后所有手机安装以裸 403 失败（CI 绿灯是虚假信心）。
**代价：每发一版 Termux 包都要改这个常量**，可用环境变量覆盖。


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

- time: 2026-10-01T05:42:30
  kind: evidence
  summary: "CI 验证（run 36819827043，6/6 job success）：android-build 确认交叉编译仍通（aarch64/linker64/四个平台库/forkpty@LIBC）；install-test 三个测试在真实 runner 上通过 —— Linux 路径仍静态、数据就位、无警告；Termux 路径（stub uname 模拟 aarch64）自动解析到 v1.1.7-termux 并装出 Android 包且无虚假警告；校验和网络错误场景确认拒绝安装。Pages 上的 install.sh 已与仓库一致（10141 bytes），线上脚本实测可装。注意 CI 是 x86_64 runner，只能验证安装链路，跑不了 arm64 二进制本身。"
  source: "2026-10-01 CI run 36819827043 全绿"
  affects: [termux-android-target]

- time: 2026-10-01T06:07:13
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]
