---
id: dictionary-repo-split
title: "词库独立仓：term-ime-dict submodule 锁定"
category: concept
status: active
tags: [dict, rime, release]
created: "2026-09-29T07:55:48"
updated: "2026-09-29T07:56:33"
---

<!-- compiled_truth -->
**结论（2026-09-29 起）**：词库数据独立仓库 [adam-ikari/term-ime-dict](https://github.com/adam-ikari/term-ime-dict)，term-ime 主仓以 git submodule 引用，tag 锁定 commit。

**边界**：词库仓只放三样 —— `luna_pinyin.dict.yaml`（70655 条字词）、`essay.txt`（297731 条词频，dict 的 `use_preset_vocabulary: true` 靠它加载，缺了候选退化成按 Unicode 排序的单字）、`opencc/`（简繁转换）。主仓 `data/rime-data/` 只留 term-ime 自己的方案配置（`*.schema.yaml`、`default.yaml`、`pinyin.yaml`）—— 模糊音裁剪逻辑改的是 schema，它跟代码强耦合，不搬。

**构建**：`CMakeLists.txt` 先把 `data/rime-data/*.yaml`（方案配置）拷进 `build/share/rime-data/`，再从 submodule `data/rime-data-dict/` 拷 dict + essay + opencc 补齐。submodule 未检出时 `message(FATAL_ERROR)` 提示 `git submodule update --init --recursive`（普通 clone 的默认状态，必须给可执行提示而不是空目录）。install 统一从 `build/share/rime-data/` 单一源装。

**运行时布局不变**：发布产物仍是 `share/term-ime/rime-data/`，tarball、install.sh 的 `PREFIX` 与 `rime-data-dir-discovery` 那页的搜索顺序全部照旧 —— 拆仓改的只是源码树里数据从哪来，二进制运行时看不出区别。

**发版关系**：词库版本号只在词库仓的 tag 上；term-ime 发版时由该 tag 的 submodule commit 决定锁哪版词库，组合关系因此隐式固定在 term-ime 的 tag 里。词库单独更新不必同步改 term-ime，反之亦然。

**CI 零改动**：所有需要构建/测试的 job（`ci.yml` build/e2e/install-test、`release.yml` build-linux）本来就带 `actions/checkout` 的 `submodules: recursive`；website/lint/release 打包 job 不碰 rime-data，不加。

**测试**：`tests/test_simplified_candidates.py` 直接读 opencc 源表（数据驱动遍历变体字），路径指向 `data/rime-data-dict/opencc/`。拆仓后第一处失败就在这里。


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
