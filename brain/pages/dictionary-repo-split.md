---
id: dictionary-repo-split
title: "词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）"
category: decision
status: active
tags: [dict, rime, release]
created: "2026-09-29T07:55:48"
updated: "2026-10-07T02:37:23"
---

<!-- compiled_truth -->
## 词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）

**结论**：`term-ime-dict` 独立仓已归档，词库与简繁数据并入
[adam-ikari/librime](https://github.com/adam-ikari/librime) 的 `dict/` 目录，
与 librime 补丁同 tag。term-ime 的 submodule 从 9 个降到 8 个。

**为什么改**：原先是三个坐标，而 librime 那侧**锁在一个没有 tag 的 commit**
（`1.16.1-10-g1d7c2618`），于是「term-ime 用的哪个 librime」无法表述，只能靠
`git describe` 反推；词库那边却只有 `v1.0.0`。两个坐标要靠人脑记住配套关系。
并成一个 tag 后不存在「装了新词库但 librime 没跟上」这种组合。

新 tag `v1.1.7-rime-stack`（= `64eda4c7`），与 term-ime v1.1.7 配套，且
master 已快进到同一 commit —— 不是停在侧分支上。pin 停在侧分支是脆弱的：
分支被删或 force-push 就会断，而 submodule 只会按 SHA 找。

## 授权（2026-10-07 修正：先前 MIT 整体声明是错的）

`dict/` **不是单一授权整体**，按文件分别：
- `luna_pinyin.dict.yaml`, `opencc/` — MIT，adam-ikari 原创
- `essay.txt` — **派生自 rime/rime-essay，LGPL-3.0**（不是 MIT）

先前 compiled_truth 说「词库是 MIT，MIT 可并入 LGPL」——这把 essay.txt 也
算进 MIT 是错的：essay.txt 是 rime-essay 数据，上游授权 LGPL-3.0。修正后
`dict/LICENSE` 改为按文件分别标注，`dict/README.md` 同步。LGPL-3.0 数据放
LGPL 的 librime fork 下本就兼容，无需额外处理；只是不能整体宣称 MIT。

**授权无冲突**：luna_pinyin.dict.yaml/opencc 是 adam-ikari 原创 MIT，与
librime LGPL 分开；essay.txt 的 LGPL-3.0 与 librime LGPL 一致。三者与
librime 本体 LGPL 协同无冲突，但 LICENSE 必须按文件分开声明。

**同步上游的代价**：本仓不再是干净的 fork。`dict/` 是上游不存在的目录，所以
`git merge upstream/master` 只会碰 librime 自己的文件。步骤写在 `dict/README.md`。

## essay.txt 升级（2026-10-07）

从 rime-essay 官方最新版更新：297731 条 → 442688 条（+48%，体积 3.7M→5.7M）。
新增词条大量是繁体高频词（一個、這個、我們、因爲…），但 `luna_pinyin_simp`
方案的 simplifier + uniquifier 把繁体候选转简体并去重，简体候选排序不退化
——`tests/test_simplified_candidates.py` 40/40 全过（你→你好、能/可/长/张/学/想/过
均简体字优先，繁体词频未干扰）。

**代价**：essay 5.7M 让首次部署编译更慢（e2e 部署段从 ~30s 变 ~137s），
加重 [[startup-readiness-window]]。体积换召回率，可接受。

## 保留不变的边界

`data/rime-data/*.yaml`（term-ime 自己的 schema 配置）**仍留在主仓** ——
模糊音裁剪逻辑改的是 schema，与代码强耦合，不搬。

`essay.txt` 是 preset vocabulary，缺了候选会退化成按 Unicode 排序的单字
（生僻字排前面）、词组出不来。


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
