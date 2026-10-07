---
id: dictionary-repo-split
title: "词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）"
category: decision
status: active
tags: [dict, rime, release]
created: "2026-09-29T07:55:48"
updated: "2026-10-07T07:34:00"
---

<!-- compiled_truth -->
## 词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）

**结论**：`term-ime-dict` 独立仓已归档，词库与简繁数据并入
[adam-ikari/librime-stl](https://github.com/adam-ikari/librime-stl)（原名
`librime`，2026-10-07 改名）的 `dict/` 目录，与 librime 补丁同 tag。term-ime 的
submodule 从 9 个降到 8 个。fork 的 parent 仍是上游 `rime/librime`。

**为什么改名 librime-stl**：fork 在上游 `rime/librime` 基础上带 term-ime 专用
补丁（drop Boost、std::regex 等），已不是纯上游；改名避免与上游同名混淆。fork
关系保留，日后 `git merge upstream` 照旧。

## 授权（2026-10-07 修正：先前 MIT 整体声明是错的）

`dict/` **不是单一授权整体**，按文件分别：
- `luna_pinyin.dict.yaml`, `opencc/` — MIT，adam-ikari 原创
- `essay.txt` — **派生自 rime/rime-essay，LGPL-3.0**（不是 MIT）

先前说「词库是 MIT，MIT 可并入 LGPL」——把 essay.txt 也算进 MIT 是错的：它是
rime-essay 数据，上游 LGPL-3.0。修正后 `dict/LICENSE` 按文件分别标注，
`dict/README.md` 同步。LGPL-3.0 数据放 LGPL 的 librime fork 下本就兼容，只是
不能整体宣称 MIT。

**授权无冲突**：luna_pinyin.dict.yaml/opencc 是 adam-ikari 原创 MIT，与 librime
LGPL 分开；essay.txt 的 LGPL-3.0 与 librime LGPL 一致。三者与 librime 本体 LGPL
协同无冲突，但 LICENSE 必须按文件分开声明。

**同步上游的代价**：本仓不再是干净的 fork。`dict/` 是上游不存在的目录，所以
`git merge upstream/master` 只会碰 librime 自己的文件。步骤写在 `dict/README.md`。

## essay.txt 升级 + 全部转简体（2026-10-07）

两步：先升级到 rime-essay 官方最新版（297731 → 442688 条，+48%），再全部
转简体并去重。最终 **437873 条**（合并 4892 条重复），体积 5.6M。

主词库 `luna_pinyin.dict.yaml` 同步转简体：**67164 条** (词,拼音) 对（去重 3495）。
两库内容现在本身已是简体，`luna_pinyin_simp` 的 simplifier 退化为幂等护栏。

**转换踩的两个坑（改词库必看）**：

1. **`乾`（gān 干燥 / qián 乾隆）与 `薹`（tái 蒜薹）是合法简体字**，不是繁体
   残留，但 opencc `t2s` 会把它们误转成 `干`/`苔`。必须逐字保护，否则输 `qian`
   出「干」而非「乾」。踩过一次：`test_simplified_candidates.py` 38/40（tai、qian
   两处 FAIL），加保护字后 40/40。
2. **一简对多繁**：`乾/幹/榦/汫` 都归 `干`，`乾` 自身还有 gan/qian 两读。转换必须
   按 (词,拼音) 对去重、同词多读音保留为多行；若只按词去重合并成单一读音，输入
   另一读音就打不出该字。

**验证**：ctest 120/120；`test_simplified_candidates.py` 40/40（繁体 0 残留，
`乾`/`薹`/`芸`/`沪`/`腺` 等合法简体字保留）；`test_e2e.py` 5/5。新增的繁体高频词
（一個、這個、我們…）转简体后与原生简体词合并，未干扰简体候选排序。

## 保留不变的边界

`data/rime-data/*.yaml`（term-ime 自己的 schema 配置）**仍留在主仓** ——
模糊音裁剪逻辑改的是 schema，与代码强耦合，不搬。

`essay.txt` 是 preset vocabulary，缺了候选会退化成按 Unicode 排序的单字
（生僻字排前面）、词组也出不来。


## Timeline

- time: 2026-09-29T07:55:48
  kind: decision
  summary: "Created this page: 词库独立仓：term-ime-dict submodule 锁定"
  source: created via brain create-page
  affects: [dictionary-repo-split]

- time: 2026-09-29T07:56:33
  kind: decision
  summary: "词库独立仓 term-ime-dict + submodule 锁定"
  source: "2026-09-29 词库拆仓会话"
  affects: [dictionary-repo-split]

- time: 2026-10-04T17:36:46
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [dictionary-repo-split]

- time: 2026-10-07T02:37:23
  kind: decision
  summary: "essay 升级到 rime-essay 官方最新版（297731→442688 条）；修正授权认知：essay.txt 是 LGPL-3.0 非 MIT"
  source: "2026-10-07 词库优化"
  affects: [dictionary-repo-split]

- time: 2026-10-07T04:03:05
  kind: note
  summary: "fork 仓已从 adam-ikari/librime 改名为 adam-ikari/librime-stl（fork 关系保留，parent 仍是 rime/librime）；.gitmodules 与 website/docs/fuzzy.md 引用同步更新"
  source: "2026-10-07 仓库改名"
  affects: [dictionary-repo-split]

- time: 2026-10-07T04:03:39
  kind: decision
  summary: "更新 compiled_truth：fork 仓已改名为 librime-stl，同步 essay 升级与授权修正"
  source: "2026-10-07 仓库改名 + essay 升级 + 授权修正"
  affects: [dictionary-repo-split]

- time: 2026-10-07T07:34:00
  kind: decision
  summary: "词库全部转简体：essay 437873 条、主词库 67164 条；两库踩坑（乾/薹 保护、一简对多繁）"
  source: "2026-10-07 清理繁体词汇"
  affects: [dictionary-repo-split]
