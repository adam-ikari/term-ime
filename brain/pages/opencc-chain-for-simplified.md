---
id: opencc-chain-for-simplified
title: "候选词简体化：链式 opencc 配置（TS + JP 变体 + 妳）"
category: decision
status: active
tags: [rime, opencc, i18n, schema]
created: "2026-09-15T10:06:20"
updated: "2026-09-16T00:10:41"
---

<!-- compiled_truth -->
## variants_ext.txt（2026-09-16）

新增 `data/rime-data/opencc/variants_ext.txt`，946 条「表外异体字 → 简体正字」映射，
挂在 t2s_full.json 链尾（第 4 步），统一收敛变体字。

来源（全部权威可再生成）：
1. Unihan `kZVariant`/`kSemanticVariant`/`kTraditionalVariant`（反演出的「异体→正字」），
   目标必须命中《通用規範漢字表》（rime-aca/character_set），且与异体字在词典里读音有交集 → 919 条。
2. Unihan `kDefinition` 里写明 "(variant of X)" 的 → 20 条。
3. 人工核对（读音完全一致、即标准简化）：邉→边 圀→国 圡→土 圗→图 兊→兑 仺→仓 辧→辨 → 7 条。

`薱 譵 嶶` 等无任何 Unihan 依据、我也无把握的，**不动**。

## 必读的坑：OpenCC 的 TextDict 不认注释行

`variants_ext.txt` 第一版带了 `#` 注释行（注释行没有 tab），OpenCC 的 `ParseKeyValues`
（deps/librime/deps/opencc/src/Lexicon.cpp:34）遇到无 tab 的行直接 `throw InvalidTextDictionary`，
整个 converter 构造失败 → rime 静默把 simplifier 关掉 → **候选全是传统字**（過 國 鍋…），

排查线索：`guo` 候选整页是 過 國 鍋（而非 过 国 锅）= simplifier 全关。
规矩：opencc 的 `type:text` 字典文件**不能有注释行、不能有空行、每行必须有 tab**。
（对比 `variants.txt`/`variants_jp.txt` 本来就没注释，所以之前没事；新增带注释的文件就炸了。）
后续若给这些 .txt 加说明，写到独立 README，不要写进文件本体。

## 回归测试改成数据驱动

`tests/test_simplified_candidates.py` 的 NON_SIMPLIFIED 改为运行时从 variants.txt / variants_jp.txt /
variants_ext.txt 读键集合（自动覆盖再生成的表），外加手写「传统字守卫」（過 國 長 學…）——
**当 simplifier 整体失效时这批字会重现而触发失败**，正好堵住这次那种"config 加载失败"的回归。

## 证据
404 音节大扫描：946 个表外异体字抽样验证 囯→国 冄→冉 懞→蒙 弾→弹 蚉→蚊 値→值 塲→场 覌→观… 全部
「变体消失 + 正字出现」；保留字 芸/沪/薹/乾/腺/你/于/曲/面 仍在。回归 40/40 + 全量绿。


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
