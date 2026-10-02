---
id: termux-android-target
title: "Termux/Android：已删除（实验性支持期间未在真机验证）"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-10-02T11:23:28"
---

<!-- compiled_truth -->
## 结论：Termux 支持已彻底删除（2026-10-02）

不只是「搁置」，是**从仓库里删干净了，并且把已发布的产物也撤了**。

### 仓库内删除范围

- `ci.yml`：`android-build` job + `install-test` 里三段 Termux 步骤
  （模拟手机安装、tag 常量守卫、裸 bootstrap 安装）
- `release.yml`：`build-android` job、termux 资产上传行、`prerelease` 逻辑
  （它存在的唯一理由就是「未验证的手机产物不能进 `/releases/latest`」）、
  tag 自动 pin 步骤（上一轮刚做的自动化，一并作废）
- `website/static/install.sh`：平台检测、prefix/tag 分支、termux 资产、
  校验段的 termux 分支、`pkg install curl` 提示
- `website/docs/termux.md`：整页删除；sidebars 条目删除
- `website/src/pages/index.tsx`：FAQ 那条、hero 提示、chip、
  结构化数据里的 `Android arm64 via Termux`
- `CMakeLists.txt`：五处 `if(ANDROID)`（依赖 toolchain 转发、opencc 宿主
  工具、`find_root_path_mode`、liblog vs libutil、`-static` 分支），
  以及解释这些分支存在的注释

验证：Linux 重新 configure + 全量重建，产物仍 **statically linked**，
114 gtest + 7 套 py e2e 全绿，install.sh 用本地镜像完整装通且
`shellcheck -S style` 干净。CI 收敛到 5 个 job，release 只发 linux x86_64/aarch64。

### 外部也已撤销（需要用户明确同意后才做的那一步，已执行）

`v1.1.7-termux` prerelease 及其 arm64 资产已删除（资产 HTTP 404），
git tag 本地与远端都已删除 —— **tag 页面本身仍在对外广告 Termux**，
只删 release 不删 tag 等于留了个指向已删内容的入口。
`v1.1.6` 的 Linux 资产与线上 install.sh 不受影响（HTTP 200）。

### 未验证就删除的理由

arm64 Android 二进制**从未在真机执行过**。bionic 只在 x86_64 模拟器上验证过，
arm64 侧是用 Linux 上的 `qemu-aarch64` + glibc 验证的。曾考虑手解 LP metadata
来提取 bionic sysroot 以补真验证，判定收益不足以抵消复杂度，未做。
即「实验性支持」这个定位本身就是准确描述，从未声称过真机可用。

## 遗留：这一轮的修复一个都没进 release（2026-10-02 收尾时发现）

删 Termux 的副作用是 `/releases/latest` 回到 **v1.1.6**，而 v1.1.6 落后 master
**48 个提交**。这一轮修的三个吞键 bug **一个都没进任何 release**，用户现在装到的
仍是坏的：

| bug | 用户可见后果 |
|---|---|
| Enter 裸转发 `\r` | **候选词永远上不了屏** —— 最严重 |
| 未消费按键被丢弃 | 中文模式 `[7` 丢掉 `7` |
| 转义序列被拆散 | Delete 提交拼音并注入字面量 `[3~` |

另有 `d7c20bb`（generated fuzzy schema 在 init 时 join，消除 prism 赛跑）同样未发布。

**注意这里有个反直觉的因果：删 Termux 本身不是 bug，但删完之后 latest 指向的
v1.1.6 里三个吞键 bug 都还在**，所以「Termux 已清理干净」不等于「发布物是干净的」。
补发版是必要的收尾动作，但打 tag 属于对外发布，需要用户明确授权，未擅自执行。


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

- time: 2026-10-02T03:28:04
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-02T11:19:16
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]

- time: 2026-10-02T11:23:28
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [termux-android-target]
