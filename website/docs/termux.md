---
id: termux
title: Termux（Android）构建
sidebar_label: Termux（Android）
sidebar_position: 8
---

# 在 Termux 上构建 term-ime

term-ime 是终端程序：它 `forkpty()` 出一个子 shell、用 termios 把终端切到 raw 模式、
再用 `TIOCGWINSZ` / `SIGWINCH` 处理窗口尺寸。这套东西在浏览器里一个都没有，所以手机上
能不能用，取决于 **Termux 提供的 PTY 能不能满足这些前提**——而不是取决于我们打包了什么。

本文只讲**怎么构建**，以及**构建出来的东西在真机上还没有验证过**。

## 当前状态（请先读这一段）

| 项目 | 状态 |
|---|---|
| 交叉编译出 arm64 Android 二进制 | ✅ 已支持，`ci.yml` 的 `android-build` job 每次推送都会验证 |
| 在真机 Termux 里跑起来 | ⚠️ **未验证**。需要一部 Android 手机手动试 |
| 通过 `install.sh` 一键安装 | ❌ 不支持。安装脚本只认 x86_64 / aarch64 的 glibc 包 |

「未验证」的具体含义：PTY 能不能 fork、raw 模式能不能进、软键盘弹起时
`SIGWINCH` 会不会来、状态栏会不会被输入法窗口挤掉——这些都需要在真实终端里看。
编译通过只说明代码能编过，不说明能用。CI job 的注释里也写了同样的提醒。

## 构建方法

需要 Android NDK（r27 及以上）与 CMake ≥ 3.20：

```bash
# 1. 取 NDK（只需要一次）
export ANDROID_NDK_ROOT=~/android-sdk
NDK_VERSION=27.1.12297006
curl -fsSL -o /tmp/ndk.zip \
  "https://dl.google.com/android/repository/android-ndk-r27-linux.zip"
unzip -q /tmp/ndk.zip -d "$ANDROID_NDK_ROOT"
export ANDROID_NDK="$ANDROID_NDK_ROOT/android-ndk-r$NDK_VERSION"

# 2. 交叉编译
cmake -B build-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android -j$(nproc) --target term-ime

# 3. 收产物：二进制 + rime-data + 翻译
mkdir -p dist/bin dist/share/term-ime/rime-data dist/share/term-ime/translations
cp build-android/term-ime dist/bin/ti
cp -r build-android/share/rime-data/* dist/share/term-ime/rime-data/
cp data/translations/*.json dist/share/term-ime/translations/
tar -czf term-ime-termux-arm64.tar.gz -C dist .
```

产物推到手机上后解开，把 `dist/bin` 加进 `PATH` 即可（Termux 默认把
`$PREFIX/bin` 放进 PATH，所以也可以直接放到 `$PREFIX/bin` 下）。

## 为什么 API 28 是下限

`deps/libuv` 的 `process.c` 用了 `posix_spawn()` 系列函数，而 bionic 是在
**API 28** 才补上这些的。低于 28 会直接编译失败：

```
error: call to undeclared function 'posix_spawnattr_init'
```

Android 8.0（API 26）以下的机器跑不了这份二进制。API 28 对应 Android 9，
覆盖面已经足够。

## 交叉编译时和 glibc 构建的差异

这些差异都写在 `CMakeLists.txt` 里，条件都是 `if(ANDROID)`，宿主构建不受影响
（宿主产物仍是全静态、BuildID 与改动前一致）。

**不是静态链接。** glibc 那条 `-static` 是本项目「单文件零依赖」卖点的一部分，
但 bionic 没有可供静态链接的 libc；强行 `-static` 出来的二进制 Android 加载器
根本不认（它仍然要求 `/system/bin/linker64` 和系统 libc）。所以 Android 侧是
动态链接，只依赖 `libc.so` / `libm.so` / `libdl.so` / `liblog.so`——这四个
平台本来就提供，Termux 不需要额外装东西。CI job 会断言依赖列表不超出这四个。

**`forkpty` 在 libc 里，不在 libutil。** glibc 下 `forkpty` 属于 libutil，所以
`target_link_libraries(... util)`；bionic 根本没有 libutil，硬链会报
`unable to find library -lutil`。`llvm-nm -D` 确认符号是 `forkpty@LIBC`。

**要链 `liblog`。** libuv 的 Android 后端会调 `__android_log_print`。

**vendored 依赖要单独转发 toolchain。** yaml-cpp / leveldb / marisa-trie / opencc
是四个**独立的 cmake 工程**（`execute_process` 另起进程配置），不会继承我们的
toolchain。交叉编译时它们会悄悄按**宿主**架构编译，最后链接时报一堆
`is incompatible with aarch64linux`。所以要把 `CMAKE_TOOLCHAIN_FILE`、
`ANDROID_ABI`、`ANDROID_PLATFORM` 显式传进去。

**NDK 的 find 路径要放开。** NDK 把 `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY` 设成
`ONLY`，`find_library` 只看 sysroot，看不到刚构建出来的 `_deps_stage`，于是
configure 报 `Could not find yaml-cpp library`——而那个 `.a` 明明就在旁边。
改成 `BOTH` 即可（sysroot 仍然优先）。

**OpenCC 的词典要用宿主工具生成。** 这是最容易卡住的一处：opencc 在**构建期**
靠**运行**自己的 `opencc_dict` 把 `.txt` 转成 `.ocd2`。交叉编译后那个工具是
Android 二进制，在这里跑不了，表现为 `opencc_dict: not found` + `Error 127`。
词典本身是与目标无关的纯数据，所以正确做法是**为宿主单独编一份** `opencc_dict`，
再把它放到 `PATH` 前面（opencc 的 `data/CMakeLists.txt` 里是
`set(OPENCC_DICT_BIN opencc_dict)`，普通 `set()` 会遮蔽我们传的 `-D` 缓存变量，
所以改变量不生效，只能走 PATH）。

## 想继续推进的话

按投入产出排序：

1. **真机验证**——找一部 Android 9+ 手机，装 Termux，解开上面的 tar 包，
   确认能进中文输入。这一步没做之前，后面所有工作的意义都只是「能编译」。
2. **软键盘适配**——手机没有物理键盘，候选栏的选词方式（方向键/数字键）在
   触屏上并不好用，可能需要一个长按或数字直选的交互。这是产品问题，不是构建问题。
3. **`install.sh` 支持 Termux**——脚本现在按 `uname -m` + glibc 假设组织，
   要支持手机得加一条 Android/Termux 分支，并在 `release.yml` 里加一个
   `build-android` matrix 产物。

试过之后欢迎反馈实际行为——尤其是 PTY 和 `SIGWINCH` 那部分，那是文档目前
唯一无法回答的问题。
