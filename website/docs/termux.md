---
id: termux
title: Termux（Android）
sidebar_label: Termux（Android）
sidebar_position: 8
---

# Termux（Android）

term-ime 的 arm64 Android 测试版本。需要在手机上用 [Termux](https://termux.dev/)
运行，需要 Android 9（API 28）及以上。

## 安装

在 Termux 里执行：

```bash
pkg install curl
curl -fsSLO https://github.com/adam-ikari/term-ime/releases/download/v1.1.7-termux/term-ime-termux-arm64.tar.gz
tar -xzf term-ime-termux-arm64.tar.gz
cp -r term-ime/* $PREFIX/
ti
```

`$PREFIX/bin` 已经在 `PATH` 里，装完直接用 `ti` 即可（`term-ime` 是兼容别名）。

这是**测试版本**，手机上遇到问题请直接开
[issue](https://github.com/adam-ikari/term-ime/issues)。

## 已知限制

- 不走首页那条一键安装命令。`install.sh` 只提供 Linux 的 x86_64 / aarch64
  glibc 包，手机上跑不了。
- 需要 Android 9 以上。

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
