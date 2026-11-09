---
id: thuocl-supplement
title: "THUOCL 长尾表：4 字起、注音生成、MIT"
category: decision
status: active
tags: [dict, license, gate]
created: "2026-11-09T22:51:26"
updated: "2026-11-09T22:52:17"
---

<!-- compiled_truth -->
## 结论：取 THUOCL 的 4 字以上子集，74938 条，作为 `luna_pinyin.dict.yaml` 的 import

`dict/thuocl.dict.yaml`（fork `adam-ikari/librime-stl` 的 `dict/`）由
`tools/build_thuocl.py` 从 vendored 的 THUNLP/THUOCL（MIT，commit `a30ce79d`）生成，
`luna_pinyin.dict.yaml` 表头 `import_tables: [thuocl]` 由 `convert_dict.py` 写入。
term-ime 的 CMake 把该文件一起复制进 `build/share/rime-data/`。

**为什么是 4 字起（有实测，不是猜）**：THUOCL 是语料切词产物，2–3 字混机构/行政碎片
（一公司、内都、源通、向县），其语料词频（万级～百万级）压过 essay 同键常用词的权重。
整表导入实测回退：`guohou` 果后>过后、`yuantong` 源通>圆通、`neidou` 内都>内斗、
`yigongsi` 一公司>一共是。取 4 字后回退消失、收益保留。95613 个 4 字以上词中
20675 个已由 luna/essay 覆盖，故产物 74938 条；词频 0 的 12.8k 行保留、权重落 1。

**收益**（同 schema、干净用户目录）：`feigongjingji` 非公经济、`xiaoweihanyang`
小尾寒羊、`wenqunadeqingruxu` 问渠哪得清如许、`chengbaojingyingquan` 承包经营权、
`shixuexingxiuke` 失血性休克、`neicunxielou` 内存泄露 —— 此前都是谐音猜测。

**代价**：`table.bin` 22.0→25.7 MB，首启部署 6.1→7.0 s，部署峰值 RSS 438→482 MB；
常用词候选顺序不变。

**注音是生成的**：THUOCL 无拼音。pypinyin 版本钉在 `dict/tools/sources.lock`
（`thuocl-pypinyin`），生成脚本断言版本不符即拒绝产出，避免多音字批次被动漂移。

**授权**：THUOCL MIT；词表+本仓生成的拼音列合起来仍按 MIT 分发，上游原文随
`dict/tools/thuocl-src/LICENSE`，`make package` 作为 `LICENSE-thuocl.txt` 入包；
term-ime 的 `LICENSES/thuocl-MIT.txt` 由 `tools/collect-licenses.sh` 生成。

**两道门都扩了**：`make check` 多比对 `thuocl.dict.yaml`；`make smoke` 部署多装一份，
`THUOCL_PROBES` 双向断言（长尾词必须出、2–3 字碎片不得压过常用词）。已验证清空产物后
三条长尾探针立即红。

## 与此前结论的关系

2026-10-07 记过「MIT 字库（pypinyin/jieba/chinese-xinhua）无可观增益、不整合」。
那次评估的是通用字/词频表；THUOCL 不同：它是按领域人工整理的词表（法律/医学/地名/
诗词/动物等），补的是长尾专名与术语，不补常用词。结论是「按 4 字起取子集」而不是
「MIT 数据一概不用」。


## Timeline

- time: 2026-11-09T22:51:26
  kind: decision
  summary: "Created this page: THUOCL 长尾表：4 字起、注音生成、MIT"
  source: "2026-11-09 THUOCL 集成"
  affects: [thuocl-supplement]

- time: 2026-11-09T22:52:17
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-11-09 THUOCL 集成"
  affects: [thuocl-supplement]
