---
id: termux-android-target
title: "Termux/Android 目标：交叉编译已支持，真机未验证"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-10-01T07:50:09"
---

<!-- compiled_truth -->
## 全新 Termux 没有任何 HTTP 客户端（2026-10-01 用户指出）

`install.sh` 里写的 `curl -fsSL ... | bash` 在**刚装好的 Termux 上直接失败**。

官方 bootstrap（`termux-packages/scripts/generate-bootstraps.sh`）只含：

    bash coreutils dash diffutils findutils gawk grep gzip less procps psmisc
    sed tar termux-core termux-exec termux-keyring termux-tools util-linux
    xz-utils

**没有 curl，没有 wget。** 这是脚本唯一一条官方安装路径的前提，但它在手机上
不成立——文档也照抄了同一条命令，用户照做会撞上 `curl: command not found`。

而且不只是取脚本这一步：`install.sh` 自己内部有 11 处 curl（版本查询、下载
tarball、取 sha256），所以就算脚本是从别处拿到的，**运行期仍然需要 curl**。

`findutils` 在 bootstrap 里，所以脚本用的 `find -type f -name ti -perm -u+x`
没问题；`tar` / `sha256sum` / `mktemp` / `install` / `ln` 都有。

## 顺带发现的第二个坑：file 也不在 bootstrap 里

装完的「校验二进制」那步调 `file`。Termux 上没有它，而 `set -o pipefail` 让
`file ... | grep -q` 整体失败，于是**每一次正确的手机安装**都以两条假警告收尾：

    install.sh: line 240: file: command not found
    !! WARNING: installed binary is not aarch64.
    install.sh: line 243: file: command not found
    !! WARNING: installed binary does not look like an Android build.

二进制完全正常，是检查工具缺席被误报成了检查失败。已用模拟手机环境
（PATH 只留 bootstrap 工具 + 伪造 `uname -m`=aarch64）实测确认改前改后的对比。

## 修法

1. 脚本开头在任何网络操作之前做 preflight：有 `curl` 用 curl，否则用 `wget`，
   两者都没有就报错并**直接给出 `pkg install curl`**（Termux）或包管理器提示
   （其他）。比在管道里炸出 `command not found` 好——后者看不到真正原因。
   三个 curl 调用点改走 `fetch` / `fetch_to`。
2. `file` 缺席时输出「跳过平台检查」并提示 `pkg install file`，不再误报。
3. 文档与首页 FAQ 改成先 `pkg install curl`，Termux 页「已知限制」里说明
   `file` 缺失导致校验跳过是正常的。

## e2e 覆盖

`ci.yml` 新增 step「Installer works on a bare Termux bootstrap」，用只含
bootstrap 工具的 PATH（无 curl / wget / file）跑完整安装：断言无客户端时报
`pkg install curl` 且**不含** `command not found`；只给 wget 时全流程走通
（含 checksum）、有 rime-data、有明确的 skip 信息、**零 WARNING**。

三条断言都用变异验证过不是空转：改提示文案、删 wget 回退、把 skip 改回误报、
整段删掉 preflight —— 四种变异都会让 step 失败。


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
