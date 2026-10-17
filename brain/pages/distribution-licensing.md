---
id: distribution-licensing
title: "分发授权义务：MIT 源码 ≠ 单一授权产物，LICENSES/ 由脚本生成"
category: decision
status: active
tags: [licensing, release, packaging]
created: "2026-10-09T16:01:54"
updated: "2026-10-17T17:41:54"
---

<!-- compiled_truth -->
**结论**：term-ime 源码 MIT，但**发行产物不是单一授权**：二进制静态链接约 15 个第三方库，随包 rime-data 混有 LGPL-3.0 / Apache-2.0 / MIT 数据。任何分发路径（GitHub Release tarball、`install.sh`、`cmake --install`）都必须随附 `LICENSE`（本项目 MIT）+ `LICENSES/`（第三方全文）+ `share/term-ime/rime-data/LICENSE`（数据逐文件授权声明）。

**LICENSES/ 由脚本生成，不手写**：`tools/collect-licenses.sh` 从 vendored submodule 里 `copy`/`slice` 出 19 个文件。理由是可审计：许可文本必须逐字来自上游，手抄会漂移。两条约束：

- 三个组件没有独立许可文件（rapidjson、utf8-cpp、X11 `keysymdef.h`），授权只写在 header 里，脚本按行号 `sed -n` 切出，并在切完后 `grep -qF` 校验切出的内容含预期标记；行号漂移时脚本**失败退出**而不是发出截断的许可。
- `LGPL-3.0.txt` 是仓库里唯一**签入**的文本（`essay.txt` 是 LGPL-3.0，但树内没有任何 dep 带这份全文）。脚本每次运行会删掉其余 `.txt` 重生成，LGPL 那份先 `mktemp` 备份再还原。不要试图从 `/usr/share/common-licenses` 取——CI runner 不保证有。

**数据授权是逐文件的**（`deps/librime/dict/LICENSE`，随包为 `rime-data/LICENSE`）：`luna_pinyin.dict.yaml` 与 `essay.txt` 为 **LGPL-3.0**（分别派生自 rime-luna-pinyin / rime-essay，非原创）；`opencc/emoji.json`、`emoji_word.txt`、`emoji_category.txt` 为 **LGPL-3.0**（rime-emoji `Emoji-14.0` 分支逐字副本，随 2026-10-13 emoji 词典加入）；`opencc/t2s.json`、`t2hk.json`、`t2tw.json`、`*.ocd2` 为 Apache-2.0（派生自 OpenCC）；`opencc/t2s_full.json`、`opencc/variants*.txt` 为 MIT（本项目原创）；term-ime 自有方案中 `pinyin.yaml` 与 `luna_pinyin*.schema.yaml` 为 LGPL-3.0（rime-luna-pinyin 副本/派生），`default.yaml` 为 BSD-3-Clause（librime 派生）。**`luna_pinyin.dict.yaml` 曾被错标**：term-ime 的 `website/docs/license.md` 与早期 brain 都写「MIT，本项目原创」，与随包的 `dict/LICENSE` 直接矛盾；fork 在 `bdbd5764`（2026-10-09「项目层改 MIT，数据按上游如实分层」）已纠正为 LGPL-3.0（派生作品不能单方面改挂 MIT），term-ime 侧到 2026-10-15 才改齐。

**为什么这条值得记住**：授权义务是分发形态的属性，不是代码的属性。改打包脚本 / 加新依赖 / 换数据来源都会静默失效它，而失败模式是发布了一个法务上不完整的包——没有编译错误，没有测试红。所以：`tests/test_rime_data.cpp` 有一条 `BundledRimeDataLicense.PerFileGrantShipsWithTheData` 钉住「数据目录必须带 LICENSE 且文本含 LGPL 与 Apache 字样」；新增第三方依赖时必须同步 `tools/collect-licenses.sh`。

**rime-data 有两份声明，因为目录由两个来源合并**：`RIME_DATA_DEST` 同时收 `deps/librime/dict/`（词库，声明在 dict/LICENSE）和本仓库 `data/rime-data/`（方案）。所以打包时落两份：`LICENSE`（=dict/LICENSE）与 `LICENSE-schemas.txt`（=data/rime-data/LICENSE，覆盖 pinyin.yaml / luna_pinyin*.schema.yaml 的 LGPL-3.0 与 default.yaml 的 BSD-3-Clause）。不合并成一份的原因：`cmake -E cat` 需要 CMake 3.18 而本项目地板是 3.14，且分开后每份许可仍可溯到来源目录。漏掉 `LICENSE-schemas.txt` 的失败模式同样是静默的——5 个方案文件随包分发却没有任何授权说明。

**重新生成**：`./tools/collect-licenses.sh`（幂等；release.yml 在打包前跑一次，所以出包的文本必然对应当次 submodule 版本）。面向用户的说明在 `website/docs/license.md`。

**`website/docs/license.md`（版本列 + 逐文件授权表）是手工维护、没有任何测试绑定的**：升 submodule pin 不会碰它，写错了既没有编译错误也没有测试红——与上面「许可文本失同步」同一类静默失败。**升 pin 的收尾清单有三项**：(1) 同步版本列（fork 自报版本 + pin 短哈希）；(2) 核对新 pin 是否新增/改变了数据文件的授权，是则同步逐文件表与「再分发须知」里的 LGPL/Apache 义务清单；(3) 重跑 `./tools/collect-licenses.sh`（`LICENSES/` 是入库文件，入库副本要跟 pin；`release.yml` 出包前会再重生一次，所以出包文本天然对应）。2026-10-12 升到 `f0dd67a8` 撞到第 1 项（fork 版本 1.16.1→1.17.0、原列的 tag 已被取代）；2026-10-15 升到 `4ea39b33` 撞到第 2、3 项——词库新增三个 LGPL-3.0 的 `opencc/emoji*`，且 `luna_pinyin.dict.yaml` 的 MIT 错标仍在，一并改齐。


## Timeline

- time: 2026-10-09T16:01:54
  kind: decision
  summary: "Created this page: 分发授权义务：MIT 源码 ≠ 单一授权产物，LICENSES/ 由脚本生成"
  source: "本轮：打包链路补许可文本"
  affects: [distribution-licensing]

- time: 2026-10-09T16:01:54
  kind: decision
  summary: "记录分发授权义务与 LICENSES/ 生成机制"
  source: "本轮：打包链路补许可文本"
  affects: [distribution-licensing]

- time: 2026-10-09T16:09:33
  kind: decision
  summary: "补：rime-data 两份声明（词库 dict/LICENSE + 方案 LICENSE-schemas.txt）"
  source: "本轮：发现方案文件授权未随包"
  affects: [distribution-licensing]

- time: 2026-10-12T06:44:52
  kind: decision
  summary: "补：license.md 版本列手工维护、无测试绑定，升 submodule pin 的收尾清单要同步它"
  source: "2026-10-12 升级 librime 子模块时发现版本列已失效"
  affects: [distribution-licensing]

- time: 2026-10-17T17:41:54
  kind: decision
  summary: "纠错+扩清单：luna 授权改 LGPL-3.0、补 emoji 数据、升 pin 收尾清单扩为三项"
  source: "2026-10-15 升 librime 到 4ea39b33"
  affects: [distribution-licensing]

- time: 2026-10-17T17:41:54
  kind: reversal
  summary: "luna_pinyin.dict.yaml 的授权从「MIT，本项目原创」纠正为 LGPL-3.0（以随包 dict/LICENSE 与 fork bdbd5764 为准）；新增 opencc/emoji* 三个 LGPL-3.0 数据；升 pin 收尾清单由「同步版本列」扩为三项：版本列 + 数据文件授权表 + 重跑 collect-licenses.sh"
  source: "2026-10-15 升 librime 到 4ea39b33：新词库加 emoji、且发现 luna 错标"
  affects: [distribution-licensing]
