---
id: termux-android-target
title: "Termux/Android 目标：交叉编译已支持，真机未验证"
category: decision
status: active
tags: [termux, android, build, cmake]
created: "2026-09-30T05:15:49"
updated: "2026-10-02T01:05:12"
---

<!-- compiled_truth -->
## 发版负担自动化（2026-10-02）

Termux 发一版原本要**手改两处**，漏掉任何一处都是静默故障：

1. `website/static/install.sh` 的 `TERM_IME_TERMUX_TAG:-v1.1.7-termux`
   —— 漏改则网站继续指向旧 tag，用户装到上一个版本，且没有任何报错。
2. `release.yml` 的 `prerelease: startsWith(github.ref, 'refs/tags/v1.1.7-termux')`
   —— 漏改则**手机产物被当作正式 release 发布**，进 `/releases/latest`，
   Linux 用户会拿到未经真机验证的 Android 二进制。

现在两处都不含版本字面量：

- prerelease 条件改为 `endsWith(github.ref, '-termux')`，按 tag 形状判断，永不需要改。
- install.sh 的常量由 release job 用 sed 改写，并在发布前断言改写生效。
  顺序是 **改写 → 发布（含 install.sh 附件）→ 提交到 master**：
  附件是改写后的那份，而提交到 master 才能让 Pages 部署（文档里那条安装命令
  实际抓的是 Pages，不是 release 附件）。

**为什么改写放在 release job 而不是 build-android**：release job 才发布
install.sh 附件，附件必须是改写后的副本。

### CI 守卫（每次 push 都跑，不等到打 tag）

`install-test` 新增一步，断言 `TERM_IME_TERMUX_TAG:-v<n>` 这个默认值仍然存在、
且 release.yml 里那条 sed 真能改写成功。因为 release job 的 grep 只在打 tag 时
才跑 —— 变量一旦被改名，要等到下一次发版才暴露，而那时 release 已经发布一半。

变异验证：把变量改名后守卫按预期失败（`::error:: ... lost its ... default`），
还原后恢复通过。


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
