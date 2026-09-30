---
id: termux
title: Termux（Android）
sidebar_label: Termux（Android）
sidebar_position: 8
---

# Termux（Android）

term-ime 的 arm64 Android 测试版本。需要在手机上用 [Termux](https://termux.dev/)
运行，需要 Android 9（API 28）及以上。

## 当前状态

| 项目 | 状态 |
|---|---|
| 交叉编译出 arm64 Android 二进制 | ✅ 已支持，`ci.yml` 的 `android-build` job 每次推送都会验证 |
| 有可下载的 arm64 预编译包 | ✅ [`v1.1.7-termux`](https://github.com/adam-ikari/term-ime/releases/tag/v1.1.7-termux) |
| 用 `install.sh` 安装 | ✅ 和 Linux 同一条命令，脚本自动识别 |

## 安装

和 Linux 用的是同一条命令，脚本会自动识别 Termux 并装对应的 Android 版本：

```bash
curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
```

装到 `$PREFIX/bin`（已在 `PATH` 里，无需 sudo），装完直接用 `ti`
（`term-ime` 是兼容别名）。

也可以指定版本号：

```bash
curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash -s -- --version v1.1.7-termux
```

这是**测试版本**，手机上遇到问题请直接开
[issue](https://github.com/adam-ikari/term-ime/issues)。

## 已知限制

- 需要 Android 9（API 28）及以上。
- 装的是动态链接的 Android 版本（bionic 没有静态 libc），依赖系统的
  `libc` / `libm` / `libdl` / `liblog`，这几个平台自带。

## 从源码编译

想跟 master 而不是上面的发布版本，就自己编。需要 Android NDK（r27+）与
CMake ≥ 3.20：

```bash
curl -fsSL --http1.1 -o /tmp/ndk.zip \
  "https://dl.google.com/android/repository/android-ndk-r27-linux.zip"
unzip -q /tmp/ndk.zip -d /opt/ndk
NDK="$(find /opt/ndk -name android.toolchain.cmake -path '*/build/cmake/*' \
      -print -quit | xargs dirname | xargs dirname | xargs dirname)"

cmake -B build-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-android -j$(nproc) --target term-ime
```

`CMakeLists.txt` 里的 Android 分支（不开 `-static`、forkpty 走 libc、vendored
依赖转发 toolchain 等）在源码里都有注释。
