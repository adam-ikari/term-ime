---
id: dictionary-repo-split
title: "词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）"
category: decision
status: active
tags: [dict, rime, release]
created: "2026-09-29T07:55:48"
updated: "2026-10-09T03:24:44"
---

<!-- compiled_truth -->
## 词库在 librime fork 的 `dict/`：既是产物目录，也是生成工具目录

**结论**：`term-ime-dict` 独立仓已归档，词库与简繁数据并入
[adam-ikari/librime-stl](https://github.com/adam-ikari/librime-stl)（原名
`librime`，2026-10-07 改名）的 `dict/` 目录，与 librime 补丁同 tag。term-ime 的
submodule 从 9 个降到 8 个（这一层到 2026-10-09 未再变）。fork 的 parent 仍是上游
`rime/librime`。

**为什么改名 librime-stl**：fork 在上游 `rime/librime` 基础上带 term-ime 专用
补丁（drop Boost、std::regex 等），已不是纯上游；改名避免与上游同名混淆。fork
关系保留，日后 `git merge upstream` 照旧。

## 2026-10-09 修正：monorepo 成立，但「rime 栈一个 tag」不再是全部

页面原标题里的「一个版本坐标」被推翻（标题无法由 brain CLI 修改，以本节为准）：

- **`dict/` 仍然是唯一词库家目录，不拆独立仓** —— 「拆独立仓」在 2026-10-09 被重新
  评估后**明确否决**（理由见 `dict-artifact-publication`）。省一次 cross-repo pin
  同步、词库与引擎补丁同 tag、本地工作树即最新，这三条理由仍成立。
- **词库多了第二个版本坐标** `dict-<VERSION>`（纯词库重发，不动引擎），因为词库现在
  **作为产物发布**，消费者可能没有 git。两个坐标的对应关系写在 `dict/README.md`。
- **新增边界：词库生成不在任何编译图里**。`dict/tools/` 靠显式 make 调用，不接 CMake；
  引擎（librime + libopencc）configure/build 不再需要 host Python，也不再执行 host
  `opencc_dict`。理由见 `opencc-data-decoupling`。fork 内 opencc 已 vendored（去
  submodule 化），fork 自己的 submodule 从 6 个减到 5 个（glog、googletest、leveldb、
  marisa-trie、yaml-cpp）。

## `dict/` 内容现在全是「产物」，生产者可复现

`dict/` 从「fork 的杂项目录」变成「词库产物目录」：只放产物（`essay.txt`、
`luna_pinyin.dict.yaml`、`opencc/**`、`LICENSE`、`VERSION`、`README.md`），生产它们的
脚本在 `dict/tools/`，上游输入 pin 在 `dict/tools/sources.lock`。复现门（重新生成并
`diff`，非空即脚本 bug，不许改数据「对齐」）见 `dict-artifact-publication`。

**底本事实（易踩）**：`luna_pinyin.dict.yaml` 的繁体底本是本仓 blob `64eda4c7` 里那一
份，**不是**上游 `rime/luna-pinyin` 的 head —— 上游已 diverge（多 `%` 权重列、prune 掉
的 115 行、新词条），拿 head 当底本 diff 不出 0。浅克隆读不到该 blob，脚本会显式报错
提示 `git fetch --unshallow`。

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

## 授权：三层，不是「词库整体 MIT」（2026-10-09 再修正）

`dict/` 按文件分别标注，共三层：

- `luna_pinyin.dict.yaml` — MIT，adam-ikari 原创。
- `essay.txt` — **派生自 rime/rime-essay，LGPL-3.0**（不是 MIT）。
- `opencc/` — **不是纯 MIT**：`TSCharacters.ocd2`/`TSPhrases.ocd2`/`HKVariants.ocd2`/
  `TWVariants.ocd2` 是从 BYVoid/OpenCC 数据**编译派生**的，那份数据 Apache-2.0；
  `t2s_full.json` 等配置是 adam-ikari 手写的 MIT。2026-10-09 补港/臺闭包时把
  Apache-2.0 一并写进 `dict/LICENSE`。

历史错误两次：①「词库是 MIT，MIT 可并入 LGPL」把 essay.txt 也算进 MIT；②补简繁闭包
时差点把派生的 `.ocd2` 继续报成纯 MIT。都已修正，LICENSE 按文件分开声明。

**授权无冲突**：MIT / LGPL-3.0 / Apache-2.0 三者与 librime 本体 LGPL 协同无冲突。

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

- time: 2026-10-07T09:29:48
  kind: note
  summary: "MIT 授权字库整合评估（2026-10-07）：pypinyin 真增量 9428 条、jieba∩pypinyin 7940 条（1.8%）且含错别字，chinese-xinhua 0 增量（已覆盖/无拼音）；rime-ice 价值最大但 GPL-3.0 被否决（改许可证）。结论：MIT 字库对候选质量无可观增益，不整合"
  source: "2026-10-07 字库源调查"
  affects: [dictionary-repo-split]

- time: 2026-10-09T03:24:35
  kind: decision
  summary: "monorepo dict/ 保留、拆独立仓重新评估后否决；推翻「一个版本坐标」——新增 dict-* 纯词库坐标；词库生成彻底移出编译图；授权修正为 MIT/LGPL-3.0/Apache-2.0 三层"
  source: "2026-10-09 词库作为产物发布（librime monorepo）"
  affects: [dictionary-repo-split]

- time: 2026-10-09T03:24:44
  kind: reversal
  summary: "推翻「rime 栈一个版本坐标」的排他性：词库作为产物发布后新增 dict-<VERSION> 第二坐标（v…-rime-stack 仍是引擎+词库配套坐标）；同时推翻 opencc/ 目录纯 MIT 的授权表述（.ocd2 派生自 Apache-2.0 的 opencc 数据）。「拆独立仓」重新评估后明确否决——monorepo dict/ 结论不变"
  source: "2026-10-09 词库产物化 Phase 1-4"
  affects: [dictionary-repo-split]
