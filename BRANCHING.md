# 分支与发布管理规范

> 2026-10-13 生效。四条分支、一张触发矩阵、一种 tag 语义。
> 仓库默认分支是 `master` —— 本规范里说的「主线」「main」都指它（GitHub 上
> 不存在叫 `main` 的分支，也不会为了名字新建一条）。

## 分支

| 分支 | 角色 | 怎么写入 | 保护 |
|------|------|---------|------|
| `master` | 主线，打过 tag 的那个 | **仅 PR 合并** | 锁直推、锁强推、锁删除 |
| `release` | 发布准备线 | **仅 PR 合并** | 同上 |
| `pre-release` | 预发布 / RC 验证线 | **仅 PR 合并** | 同上 |
| `develop` | 日常开发主线 | 可直接推送 | 不锁 |

- **日常开发**：feature 分支 → `develop`（直推即可，单人项目不摆架子）→
  想进主线就发 PR → `master`。
- **发正式版**：先把 `master` 的内容 **PR 同步**进 `release`，在上面收拢发布
  改动（changelog、版本坐标、许可页版本列这类收尾项）→ PR 合并回 `master` →
  在 **master** 上打 tag `vX.Y.Z`。
- **出 RC**：同样先 PR 同步内容进 `pre-release`，打 tag `vX.Y.Z-rc.N`。
  tag 含 `-` 时 `release.yml` 自动把 Release 标为 **prerelease** ——
  GitHub 的 `releases/latest` 只返回正式版，所以 RC 不会顶掉 `install.sh`
  取的那个 latest。RC 验完，内容经 PR 走 `release` → `master`，再打不带
  后缀的正式 tag。

> `release` / `pre-release` 是**常驻分支**，不是每次发布临时切出来的：更新它们
> （哪怕只是把 master 快进对齐）也必须走 PR —— 实测快进推送同样被规则拒绝
> （GH013）。`develop` 未锁，快进推送不受限。
>
> 更新这两条分支的姿势：**从目标分支切 helper 分支 cherry-pick 后发 PR**
> （例：`sync/align-release`）。不要拿 `master` 当 PR 的 head —— 实测 head
> 为默认分支的 PR 不触发 `pull_request` 事件（四次零 check，与过滤器无关）；
> 且 rebase 合并留下的复制历史会与 master 原始历史在 merge-base 处对同名
> 文件 add/add 冲突。helper 分支路径两则都避开（PR #8/#9 实测全绿）。

### 保护规则是什么、不是什么

用 GitHub **Rulesets**（`protect-main-release-lines`）锁 `master` / `release` /
`pre-release`：

- 必须走 PR 合并 —— **管理员也一样**，直推会被拒（施加当天实测：
  `git push origin master` 返回 ruleset 拒绝，这就是负例自检）。
- 禁 force-push、禁删分支。
- **不**要求审批数（0）：单人仓库自批没有意义，规则是「必须开 PR」，
  不是「必须别人点头」。
- **不**要求 status check 通过才能合并（GitHub 层面未强制）：绿了再合是纪律，
  不是闸门。哪天被自己的红 PR 合进去坑了，再把它升级成 required checks。
- `develop` 不锁；tag 不受分支规则影响（打 tag 照常）。

## 触发矩阵

| workflow | 触发条件 | 跑什么 |
|---|---|---|
| Build and Test（`ci.yml`） | **仅 PR，且不限目标分支**（push 不再触发） | build、ctest、11 套 e2e、website build、lint、install-test |
| Release（`release.yml`） | tag `v*`（+ 手动 dispatch） | 双 arch 构建 + tests 闸门（ctest + e2e）→ 发包 |
| Deploy Website（`website.yml`） | push master 且 `website/**` 有改动 | GitHub Pages 部署 —— 部署不是测试，不在此矩阵的门里 |

推论，照实说不美化：

- 合并进 master **没有** post-merge 测试运行，这正是「仅 PR」的含义。
  合并前的 PR run 就是那次改动的 CI 证据。
- 打 tag 后仍有 `release.yml` 的 tests 闸门兜底，发布不裸奔。
- 发布前的本地门不变：`ctest` + `./tools/run-e2e.sh`（见 TESTING.md 发布清单）。

## e2e 偶发怎么处置

`tools/run-e2e.sh` 对失败的套件**自动重试一次**，汇总打印：

- 重试才过 → `FLAKY` 出现在汇总里（**不静默**），退出码 0；
- 连挂两次 → 非零退出，当真缺陷查。

这是「连挂两次才当真缺陷查」判据的自动化形态。重试兜的是**暴露面**，不是
**根因**：`test_settings_panel_e2e` 在 v1.1.8 发布闸门上偶发失败过一次，
诊断已经补上（`poll_until` 打印停止原因 + 子进程退出状态），下次复发必须自己
报出死因。细节见 brain `e2e-harness-contract`。

## 相关

- 测试跑什么、发布前查什么：[TESTING.md](TESTING.md)
- tag 之后的出包与校验：brain `release-publish-flow`
