---
id: release-publish-flow
title: "发布流程：tag → release.yml 出包 → tarball + install.sh（v1.1.6 起停 Homebrew）"
category: concept
status: active
tags: [release, ci]
created: "2026-09-29T00:41:51"
updated: "2026-10-06T15:56:45"
---

<!-- compiled_truth -->
发布 = 打 tag 触发 CI 出包，GitHub Release 就是全部发布产物。版本号只在 tag 里，`CMakeLists.txt` 的 `project(... VERSION 1.0.0)` 只用于 SOVERSION，不随发布变化（别去改它，也别指望 `ti` 有 `--version` —— 没有）。

**v1.1.6 起不再做 Homebrew 渠道**（mac 环境不需要）：`packaging/homebrew/term-ime.rb` 已删，`adam-ikari/homebrew-tap` 不再推新版本。tarball + install.sh 是唯一安装渠道。

## 顺序

1. 先推 `master`（发布提交必须先在默认分支上，release notes 才能对比出内容）。
2. `git tag -a vX.Y.Z` 并推 tag → `release.yml` 在 x86_64 + aarch64 两个 runner 上构建全静态二进制，产出 `term-ime-linux-<arch>.tar.gz` 与 `.sha256` sidecar，连同 `website/static/install.sh` 一起发成 GitHub Release（`generate_release_notes: true`）。到此发布完成，无后续步骤。

## 摘要从哪来

`gh release view vX.Y.Z --json assets` 的 `digest` 与 release.yml 自己 `sha256sum` 出来的 `.tar.gz.sha256` sidecar 应当逐字相同；两个来源对上一次，再独立 `curl` 下载 tar 复算一次 sha256（v1.1.3 三步全对上）。只信 sidecar 或只信 API 都不够：前者证明构建自洽，后者证明 URL 真的在发那个字节。

## 网站

`website/**` 有改动时推 `master` 自动触发 `Deploy Website`（GitHub Pages，concurrency 只留最新一次）。没碰 `website/**` 的提交不会重新部署站点；要重发只能 `workflow_dispatch`。install.sh 走 Pages 固定 URL 且默认取 `releases/latest`，所以发新版**不需要**改 install.sh。

## 发布前该跑而 CI 不跑的

`release.yml` 不做任何测试，只做静态链接检查。发布前必须本地跑 `ctest` + 六套 python e2e（见 `e2e-harness-contract`；本机 load 高的时候启动就绪门会超时，那是测量假象，不是缺陷）。


## Timeline

- time: 2026-09-29T00:41:51
  kind: decision
  summary: "Created this page: 发布流程：tag → release.yml 出包 → formula 双写（仓库副本 + homebrew-tap）"
  source: "2026-09-29 v1.1.3 发布"
  affects: [release-publish-flow]

- time: 2026-09-29T00:42:13
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-29 v1.1.3 发布"
  affects: [release-publish-flow]

- time: 2026-09-29T02:26:01
  kind: evidence
  summary: "v1.1.4 发布完成（tag v1.1.4 @ b2b71a9+docs，Release run 成功）。sha256 三方一致：x86_64 5ebc864ffbebb2e857b8850c3038a414d27039a6cdf5ea06c0e92e9aee048f43、aarch64 5830bb2d457c461a5b75aded55b73bff2522bd6a3939dbd36afca8126a529770（GH 资产 digest == CI sidecar == 独立 curl 重算）。formula 双写完成，仓库副本与 homebrew-tap 副本 diff 为空。新增一步验证：把**已下载的发布 tarball** 直接放进 unshare 诱饵场景跑候选，而不是只验本地 build 产物 —— 发布出去的二进制才是用户手里的那个。"
  source: "v1.1.4 发布会话"
  affects: [release-publish-flow]

- time: 2026-09-29T03:50:20
  kind: evidence
  summary: "v1.1.5 发布完成：ctest 111/111 + 六套 e2e 全绿 → tag v1.1.5 → release.yml 3m51s 出双 arch 包 → sha256 三来源（API digest / sidecar / curl 复算）全部一致 → formula 双写。x86_64 012c0694…，aarch64 a38050d6…"
  source: "2026-09-29 v1.1.5 发布"
  affects: [release-publish-flow]

- time: 2026-09-29T03:50:53
  kind: decision
  summary: "补 tap 默认分支是 master 的坑（v1.1.5 踩到）"
  source: "2026-09-29 v1.1.5 发布"
  affects: [release-publish-flow]

- time: 2026-09-29T05:31:04
  kind: evidence
  summary: "v1.1.6 发布完成：ctest 111/111 + 七套 e2e 全绿 → tag v1.1.6 → release.yml 成功 → sha256 三来源（API/sidecar/curl 复算）一致 → formula 双写逐字节一致，tap 这次直接推 master（无误建分支）。x86_64 5c929208…，aarch64 08dc8451…。内容：模糊音 window 框分组 + label 改音标对 + 描述固定槽。"
  source: "2026-09-29 v1.1.6 发布"
  affects: [release-publish-flow]

- time: 2026-09-29T05:36:28
  kind: decision
  summary: "v1.1.6 起不做 Homebrew 渠道，发布流程去掉 formula 双写"
  source: "2026-09-29 用户决定不做 brew 渠道"
  affects: [release-publish-flow]

- time: 2026-10-06T15:56:45
  kind: note
  summary: "title 修正：去掉陈旧的 'formula 双写（仓库副本 + homebrew-tap）'，与 compiled_truth 正文（v1.1.6 起停 Homebrew）一致"
  source: "2026-10-06 现状清理"
  affects: [release-publish-flow]
