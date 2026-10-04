---
id: dictionary-repo-split
title: "词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）"
category: decision
status: active
tags: [dict, rime, release]
created: "2026-09-29T07:55:48"
updated: "2026-10-04T17:36:46"
---

<!-- compiled_truth -->
## 词库已并入 librime fork（2026-10-04，取代 2026-09-29 的独立仓方案）

**结论**：`term-ime-dict` 独立仓已归档，词库与简繁数据并入
[adam-ikari/librime](https://github.com/adam-ikari/librime) 的 `dict/` 目录，
与 librime 补丁同 tag。term-ime 的 submodule 从 9 个降到 8 个。

**为什么改**：原先是三个坐标，而 librime 那侧**锁在一个没有 tag 的 commit**
（`1.16.1-10-g1d7c2618`），于是「term-ime 用的哪个 librime」无法表述，只能靠
`git describe` 反推；词库那边却只有 `v1.0.0`。两个坐标要靠人脑记住配套关系。
并成一个 tag 后不存在「装了新词库但 librime 没跟上」这种组合。

新 tag `v1.1.7-rime-stack`（= `64eda4c7`），与 term-ime v1.1.7 配套，且
**master 已快进到同一 commit** —— 不是停在侧分支上。pin 停在侧分支是脆弱的：
分支被删或 force-push 就会断，而 submodule 只会按 SHA 找。

**授权无冲突**：词库是 MIT（adam-ikari），librime 本体是 LGPL（RIME Developers），
MIT 可并入 LGPL，且两者作者同一人。`dict/LICENSE` 单独保留，不并入 LGPL 声明。

**同步上游的代价**：本仓不再是干净的 fork。`dict/` 是上游不存在的目录，所以
`git merge upstream/master` 只会碰 librime 自己的文件。步骤写在 `dict/README.md`。

**内容逐字节一致**（并入时校验，并入前后都验过）：`luna_pinyin.dict.yaml`
sha256 `86e7df8795ce…`、`essay.txt` sha256 `3d11a425aa14…`、`opencc/` 7 个文件
`diff -rq` 全一致。归档仓的描述已改成指向新位置，tag `v1.0.0` 保留可查。

## 保留不变的边界

`data/rime-data/*.yaml`（term-ime 自己的 schema 配置）**仍留在主仓** ——
模糊音裁剪逻辑改的是 schema，与代码强耦合，不搬。所以「词库独立」指数据，
不包括方案配置。

`essay.txt` 的作用要在注释里讲清楚，它是 preset vocabulary，缺了候选会退化成
按 Unicode 排序的单字（生僻字排前面）、词组出不来 —— 这是拆仓过程中最容易丢
的信息。

## 验证

从零 configure + 全量重建（删 build/），产物 rime-data 与词库源逐字节一致。
118 gtest + 7 套 py e2e 全绿。

**另做了一次真正的裸 clone 验证**（不只是本仓库）：

    git clone https://github.com/adam-ikari/term-ime.git fresh
    git submodule update --init --recursive

干净树上 118 gtest + `test_simplified_candidates` + `test_fuzzy_pinyin` 全绿，
`deps/librime` 解析到 `v1.1.7-rime-stack` = `64eda4c7`。

这一步抓到两件只在本仓库看不见的事：

1. `git submodule update --init deps/librime`（**不带 `--recursive`**）会漏掉
   librime 自己的嵌套 submodule，configure 死在
   `deps/librime/deps/yaml-cpp does not appear to contain CMakeLists.txt`。
   报错位置离真正的原因（少 clone 了一层）很远。
2. 判断 submodule 是否就绪不能只看目录非空 —— 半途失败的 clone 会留下只有
   `.git` 的目录，`ls -A | wc -l` 看着像有内容。得查关键文件在不在
   （`CMakeLists.txt`、词库的 `essay.txt`）。

term-ime 缺 dict/ 时的报错已验过是明确的：
`Dictionary data missing at .../deps/librime/dict. Run: git submodule update --init --recursive`。


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
