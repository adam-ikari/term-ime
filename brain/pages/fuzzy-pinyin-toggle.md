---
id: fuzzy-pinyin-toggle
title: "模糊音开关：换 schema，不做补丁/重部署"
category: decision
status: active
tags: [rime, config, settings, schema]
created: "2026-09-15T08:07:37"
updated: "2026-09-16T01:33:30"
---

<!-- compiled_truth -->
## 模糊音组的落点（2026-09-16）

**规则写在 `data/rime-data/luna_pinyin_simp_fuzzy.schema.yaml` 的 `speller/algebra`**，
不是补丁、不是运行时配置。当前组：

| 组 | 规则 |
|---|---|
| zh/z | `derive/^([zcs])h/$1/` + `derive/^([zcs])([^h])/$1h$2/` |
| n/l | `derive/^n/l/` + `derive/^l/n/` |
| r/l、r/y | `derive/^r/l/`、`derive/^ren/yin/`、`derive/^r/y/` |
| hu/f | hu_f_buhun 那组字面量 |
| en/eng（含 in/ing） | `derive/([ei])n$/$1ng/` + `derive/([ei])ng$/$1n/` |
| **an/ang（含 ian/iang、uan/uang、üan/üang）** | `derive/an$/ang/` + `derive/ang$/an/` |

最后两条是 2026-09-16 补的：`an$/ang` 一条规则就同时覆盖 an/ang、ian/iang（"ian" 以 "an" 结尾）、
uan/uang、üan/üang（"van"/"üan"），不需要为每个韵母各写一条。
（`data/rime-data/pinyin.yaml` 里**没有** an_ang 组，所以这几条是本项目自写的。）

## 加新模糊音组的做法

1. 在 `luna_pinyin_simp_fuzzy.schema.yaml` 的 algebra 里追加 `derive/.../`；
2. 重新 configure+build（CMake 会把 data/rime-data 拷进 build/share）；
3. fuzzy schema 的 prism 会因源文件更新而重编译（冷启动/schema 切换时）；
4. 在 `tests/test_fuzzy_pinyin.py` 的 FUZZY_ONLY 里加一条断言（开=出现、关=消失）。

## 证据
`tests/test_fuzzy_pinyin.py` 20/20：开→ la 有那、fen 有风、fan 有方、lan 有狼、lang 有蓝、
qian 有枪、wan 有网；面板切「关」→ 全部消失且精确读音仍在；再切「开」→ 恢复。
全量回归（58 gtest + 3 C++ e2e + 3 py e2e + 简体 40/40 + 翻页 17/17 + 候选数 10/10）绿。


## Timeline

- time: 2026-09-15T08:07:37
  kind: decision
  summary: "Created this page: 模糊音开关：换 schema，不做补丁/重部署"
  source: "2026-09-15 模糊音可开关"
  affects: [fuzzy-pinyin-toggle]

- time: 2026-09-15T08:07:37
  kind: decision
  summary: "模糊音在设置面板开关（配置键 fuzzy_pinyin，默认开）；实现方式是切换到 <schema>_fuzzy（目前 luna_pinyin_simp_fuzzy），基础 schema 不再硬编码模糊规则。"
  source: "2026-09-15 模糊音可开关"
  affects: [fuzzy-pinyin-toggle]

- time: 2026-09-16T01:33:30
  kind: decision
  summary: "模糊音开关（设置面板 + fuzzy_pinyin 配置，默认开）= 在 luna_pinyin_simp（精确）与 luna_pinyin_simp_fuzzy（模糊）间切 schema。模糊规则写在 fuzzy schema 的 speller/algebra 里。"
  source: "2026-09-16 补 an/ang"
  affects: [fuzzy-pinyin-toggle]
