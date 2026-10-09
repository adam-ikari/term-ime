---
id: dict-artifact-publication
title: "词库作为产物发布：两个版本坐标、复现门与资产命名"
category: decision
status: active
tags: [dict, release, artifact, versioning]
created: "2026-10-09T03:16:50"
updated: "2026-10-09T05:21:53"
---

<!-- compiled_truth -->
## 词库留在 monorepo，但它是产物不是源码目录

`dict/` 在 librime fork（`adam-ikari/librime-stl`）根目录，与引擎补丁同仓；
**不拆独立仓**（拆过一次、`term-ime-dict` 已归档，理由见 [[dictionary-repo-split]]）。
本轮（2026-10-09）推翻的是另一件事：**「一个版本坐标」的排他性**。

现在两个坐标各管一件事：

| 坐标 | 内容 | 谁用 |
|---|---|---|
| `v…-rime-stack` | 引擎 + 词库配套 | term-ime 走 submodule 锁 commit |
| `dict-<VERSION>` | 纯词库重发，不动引擎 | 只要数据的消费者；版本号读 `dict/VERSION` |

两个坐标不能混用：`dict-*` 不含引擎，stack tag 才是配套物。`dict/README.md`
里给每个 `dict-*` 标了最低可用 stack —— 词库数据依赖引擎侧的解析行为
（`use_preset_vocabulary`、`shared_data_dir/opencc`），旧引擎配新词库会静默退化。

term-ime **不**在构建时下载产物，也不把词库拷进自己仓里：继续读
`deps/librime/dict/`。理由是 clone-and-build 必须离线可用；产物面向仓外消费者。

## 复现门是唯一的验收

`make -C dict/tools check` 重新生成三份产物并与已提交字节比对：
`essay.txt`、`luna_pinyin.dict.yaml`、`opencc/`。**diff 非空就是脚本错**，
不许改数据去「对齐」。此前这两个转换从未有脚本进仓（只有 commit message 里的
描述），所以「作为产物发布」的第一步是把生产者变成可复现的东西。

已验证的契约（逐条实测，不是推断）：

- essay：上游 `rime/rime-essay@054920de`（442688 行）→ 逐字转换 → 按词去重取
  最大权重 → 码位排序 = **437873 条，0 字节差**。
- 主词库：底本是本仓 `64eda4c7` 里那份繁体快照（70655 条），**不是**上游当前
  head（上游此后加了读音权重、日文国字与注音索引，本仓又剪过 115 行）。
  表头注释不转换、权重列剥离、按 (词,拼音) 去重、整行排序 = **67164 条，0 字节差**。
- 4 个 `.ocd2` 由 opencc@`556ed224` 的表生成，字节一致。

转换必须**只做逐字**（`tools/t2s_char.json` 只挂 `TSCharacters.ocd2`）。带上词组表
会把词换掉（家俱→家具），带上异体表会把生僻码位折到常用字（㐀→丘）：词库要的
是字符级重编码，不是用词规范化。运行时那条 `t2s_full.json` 链是给上屏文本用的，
两件事不能混 —— 这也是 `t2s_full.json` 与 `t2s.json` 并存的原因。

## 资产与校验

`term-ime-dict-<ver>.tar.gz`（数据与平台无关，名字不带 arch）解包根即安装形态
`share/term-ime/rime-data/`，附 `.sha256` sidecar，沿用 [[release-publish-flow]]
的三方校验（API digest / sidecar / 独立 curl 复算）。`VERSION` 与 tag 不符时
workflow 直接失败，防发错号。

**对外破坏性变更**：`rime-deps-*` 资产从此不含 `share/opencc/**` —— 那是 opencc
装机数据，本来就是引擎构建的副作用。依赖它的消费者要改拿 `term-ime-dict-*`。
发布说明必须写明。

授权按文件分层，`dict/LICENSE` 三份并列（MIT 原创部分 / essay 的 LGPL-3.0 /
opencc 派生的 Apache-2.0）。整个 `dict/` 不能整体宣称 MIT。


## Timeline

- time: 2026-10-09T03:16:50
  kind: decision
  summary: "Created this page: 词库作为产物发布：两个版本坐标、复现门与资产命名"
  source: "2026-10-09 词库产物发布设计"
  affects: [dict-artifact-publication]

- time: 2026-10-09T03:16:50
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-10-09 词库产物发布设计"
  affects: [dict-artifact-publication]

- time: 2026-10-09T05:21:53
  kind: decision
  summary: "工具链进仓 a0b59372：复现门 make check 离线全绿（essay 437873 / luna_pinyin 67164 / opencc 三产物字节复现）。essay 繁体源改从 git 历史取（sources.lock essay-base），原网络抓取会截断且缓存盲信，破坏复现"
  affects: [dictionary-repo-split, opencc-data-decoupling]
