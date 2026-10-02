---
id: termux-android-target
title: "Termux/Android 目标：交叉编译已支持，真机未验证"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-10-02T02:48:22"
---

<!-- compiled_truth -->
## Termux：实验性支持，就此搁置（2026-10-02 用户定）

**Termux 定位为实验性支持，工作到此为止。** 不要在没有新指示时重启它。

搁置时的状态（不是「已验证」，是「已知的已知」）：

| 项 | 状态 |
|---|---|
| arm64 Android 二进制能否交叉编译 | ✅ CI 每次推送验证 |
| 产物能否下载安装 | ✅ release 资产 + install.sh，实测装过 |
| 发版是否还要手改 | ✅ 已自动化，两处版本字面量都去掉了 |
| **arm64 Android 二进制能否实际运行** | ❌ **从未执行过** |
| 交互层（软键盘/窗口遮挡/长按选词） | 明确不在范围内 |

唯一的技术空白是「arm64 编码 × bionic 运行时」这个组合没跑过。两个维度分别
验证过（bionic 侧用 x86_64 Android 产物在模拟器里真跑；arm64 侧用 Linux/glibc
交叉编译 + qemu-aarch64 真跑），所以残差风险判断为小。为它去挖 system 镜像的
LP 元数据手工抽 bionic sysroot，属于对实验性目标的过度投入 —— 已评估，不做。

下次有人提 Termux，先确认他是否知道这一条：**它能装、能跑起来这件事本身还没
被验证过**，只是在两个维度上分别间接验证过。

已知的范围外事项（提过、被明确排除，不要再翻出来做）：
- 设置面板加「中/英文模式」项（软键盘没 Ctrl → 范围外，只面向键盘设备）
- 软键盘交互适配
- 为手机缺失的物理键补绑定


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

- time: 2026-10-02T00:29:25
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-02T00:41:58
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-02T01:05:12
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-02T02:48:07
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-02T02:48:22
  kind: decision
  summary: "Termux 收尾，工作区干净，master 已推送。当前 master 顶部是 5b84240（SIGWINCH 断言）。CI run 36955323672 全绿（6/6 job），e2e 含 resize 四条断言。fuzz 驱动的活性探测（十行）**未做** —— 用户质疑必要性后我判断收益仅限手动工具，且 5000 动作未出现挂起，代码内循环均有界。这是主动放弃，不是遗漏。"
  source: "2026-10-02 收尾"
  affects: [termux-android-target]
