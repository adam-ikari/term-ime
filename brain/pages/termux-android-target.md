---
id: termux-android-target
title: "Termux/Android 目标：交叉编译已支持，真机未验证"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-10-01T10:46:54"
---

<!-- compiled_truth -->
## arm64 二进制曾经「从未被执行过」——这个结论是错的（2026-10-01 更正）

之前记的「本地只有 x86_64 镜像，无 arm64 镜像，故实测用的是 x86_64 ABI 产物」
把**没下载**当成了**不存在**。`sdkmanager --list` 里 arm64 镜像一直有：

    system-images;android-31;default;arm64-v8a
    system-images;android-31;aosp_atd;arm64-v8a      # 用这个，体积小、无 Google 依赖

但**模拟器这条路仍然走不通**，原因是硬限制而非缺文件：

    FATAL | Avd's CPU Architecture 'arm64' is not supported by the QEMU2
           emulator on x86_64 host. System image must match the host architecture.

即 x86_64 宿主 + `-accel off` 无法跑 arm64 guest。

## 可行替代：用 qemu-user 跑 aarch64 Linux 构建（已验证有效）

**关键洞察**：要验证的是「arm64 指令集上代码是否正确」，不必非得是 Android。
装 `g++-aarch64-linux-gnu` 后可以交叉编出一份 **aarch64 glibc** 的 term-ime，
再用 `qemu-aarch64-static` 真跑起来。已实测：

- 无 TTY 优雅退出（`启动失败 / 初始化失败` 面板，退出码 1，不崩溃）
- 真 PTY 下完整流程：`nihao` → 候选栏 `1.你好 2.利好 3.立好 4.理好 5.立号`
  → Enter 提交 → 再 Enter shell 报 `not found`
- **整套 e2e 在 arm64 上跑**：fuzzy 24/24、e2e 5/5、settings 6/6、
  settings_panel 8/8、punctuation 7/7、simplified/paste PASS。
  含 per-combination schema 生成 + prism 部署（当初 `close()` 那个 bug 的位置）。

所以 arm64 从「完全没验证」变成「代码路径全部验证过」，剩下的只有
**bionic 特有的运行时行为**（真机软键盘、输入法窗口遮挡、长按选词）仍需真机。

### 两个必须知道的坑

1. **qemu-user 不能链式 exec aarch64 二进制**：让 arm64 的 harness 去
   exec 另一个 arm64 程序会 `Exec format error`（没注册 binfmt_misc）。
   正确做法是让 **x86_64 宿主 harness forkpty 后 exec qemu 本身**，
   把 arm64 程序作为 qemu 的参数。

2. **交叉构建 opencc 会死在 Error 127**：opencc 的 `.ocd2` 词典是**构建期**
   跑自己的 `opencc_dict` 生成的，交叉编译时那个工具是目标架构、跑不了。
   CMakeLists 里已有解法（`_deps_build/opencc-host` + PATH 前置），
   但它被 `if(ANDROID)` 挡住了。
   做 aarch64-linux 实验时手工复用即可：把 Android 构建产出的
   `_deps_build/opencc-host/src/tools` 前置到 PATH 再 configure。

   注意这不影响已发布配置：CI 的 linux-aarch64 用的是**原生 ARM64 runner**
   （`ubuntu-22.04-arm`），不是交叉编译，所以 `ANDROID` 门控对现有发布矩阵是够的。

## arm64 产物本身的静态核对（也已做）

- `llvm-readelf -d`：NEEDED 只有 `liblog/libdl/libm/libc`，解释器 `/system/bin/linker64`
- 逐个静态库的 `.o` 核对目标架构时，注意两类假警报：
  - 项目自己的 `.a` 里装的是 **LLVM bitcode**（LTO），`llvm-readelf` 读不了，
    要看 `llvm-dis` 出来的 `target triple = aarch64-none-linux-android28`
  - `_deps_build/opencc-host/**` 是 x86_64 宿主工具，**本就不该**链进目标二进制。
    实际链入的是 `_deps_stage/lib/libopencc.a` 与 `libmarisa.a`，
    两者都是 AArch64（已核实）


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

- time: 2026-10-01T06:36:44
  kind: evidence
  summary: "修正上一条 evidence 的过头表述：模拟器里「nihao → 候选栏显示 1.你好」是实测成立的，但「提交成功」不成立——回车那次是 0 字节。进一步用同一 harness 在 Linux 上做对照，确认这是 term-ime 自身的 Enter 缺陷（见 [[enter-does-not-commit-composition]]），不是 Android 平台问题，也不是我 harness 的问题。模拟器验证的准确范围：二进制可加载执行、forkpty 正常、librime 全流程、per-combination schema 生成、候选栏渲染正确、无 TTY 时优雅退出。提交链路与真机交互仍未通过。"
  source: "2026-10-01 逐帧 harness 复测"
  affects: [termux-android-target, enter-does-not-commit-composition]

- time: 2026-10-01T07:35:39
  kind: evidence
  summary: "Enter 修复在 Android 上确认生效，逐帧证据：nihao → 候选栏 1.你好 2.利好 3.立好 4.理好 5.立号；Enter 那帧 296 字节且「你好」进入 shell 输入行；再按一次 Enter shell 执行「你好」报 inaccessible or not found —— 即「Enter 只提交、再按一次才执行」，与 fcitx5/ibus+rime 及 Linux 行为一致。修复前该场景第二次 Enter 执行的是空行、什么都不发生。模拟器实测的坑：软件模拟（无 KVM）下 leveldb 重编 70k 词条要几分钟，调试时应复用已编译的 build/ 目录，否则会误判成「词典没部署」。"
  source: "2026-10-01 修复后 Android 模拟器复测（x86_64 ABI, API 31）"
  affects: [termux-android-target, enter-does-not-commit-composition]

- time: 2026-10-01T07:50:09
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-01T10:46:54
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]
