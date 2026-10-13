---
id: branch-release-governance
title: "分支与发布管理规范：四分支 + PR-only 锁定 + 触发矩阵"
category: decision
status: active
tags: [release, ci, branching]
created: "2026-10-13T02:36:06"
updated: "2026-10-13T02:36:06"
---

<!-- compiled_truth -->
分支与发布的治理（2026-10-13 起生效）。仓库默认分支是 master —— 规范里说的「主线」「main」都指它；GitHub 上没有叫 main 的分支，也不为名字新建一条。规范文档在 repo `BRANCHING.md`，本页是它的持久记忆。

## 四条分支

- `master`：主线，正式 tag 打在这里。**仅 PR 合并**。
- `release`：发布准备线（收拢 changelog、版本坐标、许可页版本列这类收尾项）。**仅 PR 合并**；完成后 PR 回 master，再在 master 打正式 tag。
- `pre-release`：RC 验证线。**仅 PR 合并**；tag 打 `vX.Y.Z-rc.N`。
- `develop`：日常开发主线，可直接推送，不锁。

## 保护规则

GitHub Ruleset `protect-main-release-lines`（id 24965586）锁 master/release/pre-release：

- 必须走 PR，**管理员同样**（`current_user_can_bypass: never`）；禁 force-push、禁删分支。
- 审批数 0 —— 规则是「必须开 PR」，不是「必须别人点头」（单人仓库自批无意义）。
- **不**要求 status check 绿才能合并：GitHub 层面未强制，「绿了再合」是纪律；哪天被自己的红 PR 坑了再升级成 required checks。
- develop 不锁；tag 不受分支规则影响。

负例自检（施加当天实测，不是配置完就算数）：`git commit-tree` 空提交推 master → 远端拒收 `GH013: Changes must be made through a pull request`。第一次尝试没测到规则：zsh 把 `$EMPTY:refs/...` 的 `:r` 当参数修饰符吃掉了 refspec，随后变量没跨 shell 保留又把推送变成了删分支 —— 两条教训合并成一条：**负例自检要在同一 shell 里重建变量，refspec 用 `${VAR}:` 花括号形式**。

## 触发矩阵

| workflow | 触发 | 跑什么 |
|---|---|---|
| ci.yml Build and Test | **仅 PR**（2026-10-13 起 push 不再触发） | build、ctest、11 套 e2e、website build、lint、install-test |
| release.yml Release | tag `v*` + dispatch | 双 arch 构建 + tests 闸门（ctest + e2e）→ 发包；tag 含 `-` → 标 prerelease |
| website.yml Deploy | push master 且 `website/**` 有改动 | Pages 部署（部署不是测试，不在门里） |

照实说推论：合并进 master **没有** post-merge 测试运行 —— PR run 就是那次改动的 CI 证据；发布门是本地 ctest + `run-e2e.sh` 与 release.yml 的 tests 闸门，不变。

## RC 不得顶掉 latest

`release.yml` 对含 `-` 的 tag 自动 `prerelease: true`。GitHub 的 `releases/latest` 只返回正式版，而 install.sh 取的正是 latest —— 不做这条，pre-release 分支的 RC 会直接变成所有人的安装来源。RC 验完经 PR 进 release/master，再打不带后缀的正式 tag。


## Timeline

- time: 2026-10-13T02:36:06
  kind: decision
  summary: "Created this page: 分支与发布管理规范：四分支 + PR-only 锁定 + 触发矩阵"
  source: created via brain create-page
  affects: [branch-release-governance]

- time: 2026-10-13T02:36:06
  kind: decision
  summary: "四分支角色、三条受保护分支必须 PR、触发矩阵、RC 不顶 latest"
  source: "2026-10-13 分支发布治理落地"
  affects: [branch-release-governance]
