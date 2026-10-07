---
id: dictionary-repo-split
title: "词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）"
category: decision
status: active
tags: [dict, rime, release]
created: "2026-09-29T07:55:48"
updated: "2026-10-07T08:18:50"
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

## essay.txt 是候选主力源，不只是赋权（2026-10-07 修正认知）

**这是决定词库能不能裁剪的关键事实。** 先前把它当「词频表」低估了它的作用。

`EntryCollector::Collect`（`deps/librime/src/rime/dict/entry_collector.cc:144-153`）
在 Pass 2 末尾遍历 preset vocabulary，对每个词条 `encoder->EncodePhrase()` —— essay
里的词**直接进候选集**，不是只给主词库已有词赋权。

实测数据（2026-10-07，转简体后）：

- 主词库 `luna_pinyin.dict.yaml` 67164 条里，**1 字词 45409、2 字词仅 11382**，
  且**不含「你好」「我们」「怎么」「可以」**。
- 这些常用词全在 essay 里。essay 437873 条中有 **91.2% 不在主词库** —— 但它们
  恰恰是常用词候选，不是冗余。

**推论：裁剪 essay 不可行。** 曾评估过两条裁剪路径，都被这条事实否掉：

- 删「主词库没有」的 91.2% 孤儿条目 → 删掉所有常用词组候选，「你好」打不出来。
- 删低权重条目（权重<100 占 38.7%、约一半体积）→ 低权重词**同样进候选集**，
  删了打不出，只是排不到前面。测试锚点里权重最低的是「腺」1048，阈值取 100
  虽然保住了测试，但代价是丢掉 16.9 万条合法候选。

essay.txt 的 5.6M 是必要的，不是冗余。

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
`乾`/`薹`/`芸`/`沪`/`腺` 等合法简体字保留）；`test_e2e.py` 5/5；`test_fuzzy_pinyin.py`
38/38；`test_punctuation.py` 7/7。

## 冷启动实测 6.5s，不是痛点（2026-10-07 修正）

先前记「essay 5.7M 让首次部署变慢，e2e 部署段从 ~30s 变 ~137s」——**那是 e2e
测试的累计耗时，不是部署成本**，测量方式错了。

实测（PTY、临时 HOME、触发完整部署编译 prism+table+reverse）：**6.5 秒**，
产物落在 `~/.local/share/term-ime/build/*.bin`。warm start 更快。

所以 essay 体积大确实增加编译时间，但没有到不可接受的程度；不值得为它改 librime
的部署机制（`.bin` 编译进**用户目录**，共享 rime-data 里预置 `.bin` 不会被复用）。

## 保留不变的边界

`data/rime-data/*.yaml`（term-ime 自己的 schema 配置）**仍留在主仓** ——
模糊音裁剪逻辑改的是 schema，与代码强耦合，不搬。


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

- time: 2026-10-07T08:18:50
  kind: decision
  summary: "修正 essay 定位：它是候选主力源不是仅赋权；裁剪不可行。实测冷启动 6.5s"
  source: "2026-10-07 优化可行性调查"
  affects: [dictionary-repo-split]
