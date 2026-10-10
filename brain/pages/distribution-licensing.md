---
id: distribution-licensing
title: "分发授权义务：MIT 源码 ≠ 单一授权产物，LICENSES/ 由脚本生成"
category: decision
status: active
tags: [licensing, release, packaging]
created: "2026-10-09T16:01:54"
updated: "2026-10-09T16:09:33"
---

<!-- compiled_truth -->
**结论**：term-ime 源码 MIT，但**发行产物不是单一授权**：二进制静态链接约 15 个第三方库，随包 rime-data 混有 LGPL-3.0 / Apache-2.0 / MIT 数据。任何分发路径（GitHub Release tarball、`install.sh`、`cmake --install`）都必须随附 `LICENSE`（本项目 MIT）+ `LICENSES/`（第三方全文）+ `share/term-ime/rime-data/LICENSE`（数据逐文件授权声明）。

**LICENSES/ 由脚本生成，不手写**：`tools/collect-licenses.sh` 从 vendored submodule 里 `copy`/`slice` 出 19 个文件。理由是可审计：许可文本必须逐字来自上游，手抄会漂移。两条约束：

- 三个组件没有独立许可文件（rapidjson、utf8-cpp、X11 `keysymdef.h`），授权只写在 header 里，脚本按行号 `sed -n` 切出，并在切完后 `grep -qF` 校验切出的内容含预期标记；行号漂移时脚本**失败退出**而不是发出截断的许可。
- `LGPL-3.0.txt` 是仓库里唯一**签入**的文本（`essay.txt` 是 LGPL-3.0，但树内没有任何 dep 带这份全文）。脚本每次运行会删掉其余 `.txt` 重生成，LGPL 那份先 `mktemp` 备份再还原。不要试图从 `/usr/share/common-licenses` 取——CI runner 不保证有。

**数据授权是逐文件的**（`deps/librime/dict/LICENSE`，随包为 `rime-data/LICENSE`）：`luna_pinyin.dict.yaml`、`opencc/t2s_full.json`、`opencc/variants*.txt` 为 MIT（本项目原创）；`essay.txt` 为 LGPL-3.0（派生自 rime-essay）；`opencc/t2s.json`、`t2hk.json`、`t2tw.json`、`*.ocd2` 为 Apache-2.0（派生自 OpenCC）；term-ime 自有方案中 `pinyin.yaml` 与 `luna_pinyin*.schema.yaml` 为 LGPL-3.0（rime-luna-pinyin 副本/派生），`default.yaml` 为 BSD-3-Clause（librime 派生）。

**为什么这条值得记住**：授权义务是分发形态的属性，不是代码的属性。改打包脚本 / 加新依赖 / 换数据来源都会静默失效它，而失败模式是发布了一个法务上不完整的包——没有编译错误，没有测试红。所以：`tests/test_rime_data.cpp` 有一条 `BundledRimeDataLicense.PerFileGrantShipsWithTheData` 钉住「数据目录必须带 LICENSE 且文本含 LGPL 与 Apache 字样」；新增第三方依赖时必须同步 `tools/collect-licenses.sh`。

**rime-data 有两份声明，因为目录由两个来源合并**：`RIME_DATA_DEST` 同时收 `deps/librime/dict/`（词库，声明在 dict/LICENSE）和本仓库 `data/rime-data/`（方案）。所以打包时落两份：`LICENSE`（=dict/LICENSE）与 `LICENSE-schemas.txt`（=data/rime-data/LICENSE，覆盖 pinyin.yaml / luna_pinyin*.schema.yaml 的 LGPL-3.0 与 default.yaml 的 BSD-3-Clause）。不合并成一份的原因：`cmake -E cat` 需要 CMake 3.18 而本项目地板是 3.14，且分开后每份许可仍可溯到来源目录。漏掉 `LICENSE-schemas.txt` 的失败模式同样是静默的——5 个方案文件随包分发却没有任何授权说明。

**重新生成**：`./tools/collect-licenses.sh`（幂等；release.yml 在打包前跑一次，所以出包的文本必然对应当次 submodule 版本）。面向用户的说明在 `website/docs/license.md`。


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
