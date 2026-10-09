---
id: opencc-chain-for-simplified
title: "候选词简体化：链式 opencc 配置（TS + JP 变体 + 妳）"
category: decision
status: active
tags: [rime, opencc, i18n, schema]
created: "2026-09-15T10:06:20"
updated: "2026-10-09T03:29:58"
---

<!-- compiled_truth -->
**opencc 变体字表 `variants_ext.txt` 的位置变了两次，现在在 librime fork 里。**

946 条「表外异体字 → 简体正字」映射，由 `opencc-chain-for-simplified` 这条链消费：
候选栏挂 simplifier，输出简体。

位置变迁（2026-10-04 之前的一切路径引用都已失效）：

| 时间 | 位置 |
|---|---|
| v1.1.0 及以前 | 主仓 `data/rime-data/opencc/variants_ext.txt` |
| 2026-09-29 ~ 2026-10-04 | 词库独立仓 submodule `data/rime-data-dict/opencc/` |
| **现在** | **`deps/librime/dict/opencc/variants_ext.txt`**（librime fork 的 `dict/` 目录，与词库、librime 补丁同 tag `v1.1.7-rime-stack`） |

**读这张表时不要按路径去找**：现在它跟着 librime 走，不在主仓的 `data/rime-data/`
下。构建时由 `CMakeLists.txt` 的 `DICT_DIR`（= `deps/librime/dict`）拷进
`build/share/rime-data/opencc/`。

`tests/test_simplified_candidates.py` 直接读源表（数据驱动遍历变体字），所以它是
这条路径的回归哨兵 —— 拆仓改路径时它是第一处会失败的地方。


## Timeline

- time: 2026-09-15T10:06:20
  kind: decision
  summary: "Created this page: 候选词简体化：链式 opencc 配置（TS + JP 变体 + 妳）"
  source: "2026-09-15 非简体候选修复"
  affects: [opencc-chain-for-simplified]

- time: 2026-09-15T12:05:59
  kind: decision
  summary: "zh_hans 用链式 opencc 配置 t2s_full.json（链序：variants_jp → TS 组 → variants，变体表放链尾收敛）；变体表需守卫，否则会把合法简体字转错。"
  source: "2026-09-15 非简体候选修复（修正）"
  affects: [opencc-chain-for-simplified]

- time: 2026-09-15T15:03:45
  kind: decision
  summary: "候选只出简体：① zh_hans 用链式 opencc（t2s_full.json）处理繁/日文新字体/异体；② 词典剪掉日文国字与注音符号条目（115 行）。"
  source: "2026-09-15 剪国字/注音"
  affects: [opencc-chain-for-simplified]

- time: 2026-09-16T00:10:41
  kind: decision
  summary: "候选只出简体：链式 opencc（t2s_full.json，链尾收敛）+ 词典剪国字/注音 + variants_ext.txt 把 946 个表外异体字映射到正字。"
  source: "2026-09-16 表外异体字"
  affects: [opencc-chain-for-simplified]

- time: 2026-10-04T23:08:04
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [opencc-chain-for-simplified]

- time: 2026-10-09T03:29:58
  kind: evidence
  summary: "2026-10-09：dict/opencc/ 现在是完整自足的运行时闭包（t2s/t2s_full/t2hk/t2tw + TSCharacters/TSPhrases/HKVariants/TWVariants + variants*.txt），所有 schema 引用的 opencc_config 都在其中；fork configure 期遍历 *.json 的 file 引用做 FATAL_ERROR 断言，term-ime 加 gtest 扫 schema 断言产物齐备。PKGDATADIR（烧死构建机绝对路径）不再是运行时输入——unshare -rm 抹掉 build/_deps_stage/share/opencc 后港/臺切换仍出候选"
  source: 2026-10-09 Phase 1
  affects: [opencc-chain-for-simplified]
