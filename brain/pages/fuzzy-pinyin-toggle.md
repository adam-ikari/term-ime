---
id: fuzzy-pinyin-toggle
title: "模糊音开关：换 schema，不做补丁/重部署"
category: decision
status: active
tags: [rime, config, settings, schema]
created: "2026-09-15T08:07:37"
updated: "2026-09-30T03:53:27"
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
| **an/ang（含 ian/iang、uan/uang、üan/üang）** | `derive/an$/ang/` + `derive/^ang$/an/` |

最后两条是 2026-09-16 补的：`an$/ang` 一条规则就同时覆盖 an/ang、ian/iang（"ian" 以 "an" 结尾）、
uan/uang、üan/üang（"van"/"üan"），不需要为每个韵母各写一条。
（`data/rime-data/pinyin.yaml` 里**没有** an_ang 组，所以这几条是本项目自写的。）

## 加新模糊音组的做法

1. 在 `luna_pinyin_simp_fuzzy.schema.yaml` 的 algebra 里追加 `derive/.../`；
2. 重新 configure+build（CMake 会把 data/rime-data 拷进 build/share）；
3. fuzzy schema 的 prism 会因源文件更新而重编译（冷启动/schema 切换时）；
4. 在 `tests/test_fuzzy_pinyin.py` 的 GROUP_PROBES 里加一条断言（开=出现、关=消失）。

## 写生成 schema 的硬约束：close() 之后才能 deploy（2026-09-30 修复 CI 长期红）

`ensure_fuzzy_schema()` 写完 `luna_pinyin_simp_fuzzy_<sig>.schema.yaml` 之后，
**必须 `out_file.close()` 再调 `deploy_schema`**。原因是两条叠在一起：

1. `std::ofstream` 在 close/destructor 之前不保证已落盘，缓冲阈值由 libstdc++ 决定；
2. `deploy_schema` 是**同步**的（librime `RunTask` 在调用线程上跑 `SchemaUpdate`），
   它读的正是刚写的这个文件。

缓冲没落盘 → librime 读到空 schema → `SchemaUpdate::Run` 在
`config->LoadFromFile` 处返回 false → `deploy_schema` 返回 false → prism 不存在。
**本机 gcc 11.4 / `__GLIBCXX__=20230528` 对 ≥1024 字节的单次 `<<` 会提前落盘，
CI runner 的工具链不是这个阈值** —— 所以本地必绿、runner 必红。

配套的两条纪律：

- **deploy 的返回值必须检查**。丢弃它，失败与成功就走同一条路，日志里什么都不留，
  症状看起来与「还在编译」一模一样（这正是排查被带偏的地方）。
- **本项目构建里 librime 的 `LOG(ERROR)` 是被编译掉的**（`CMakeLists.txt` 的
  `ENABLE_LOGGING=OFF`），所以应用自己的日志是唯一证据源；e2e 失败时要 dump
  rime 用户目录（含 `build/`）与日志尾部。

顺带否掉一个错误结论：`deploy_schema` 之后调 `join_maintenance_thread()` 是 no-op
（`std::async` 的 future 在第一次 `get()` 后 `valid()` 已为 false），它不是竞态的来源。

## 证据
`tests/test_fuzzy_pinyin.py` 19/19（本地与 CI 皆绿）：开→ la 有那、fen 有风、fan 有方；
面板切「关」→ 对应字消失且精确读音仍在；再切「开」→ 恢复。
全量回归：111 gtest + 6 套 py e2e 全绿；CI run 36664971087 五个 job（lint/website/
build/install-test/e2e）全 success。


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

- time: 2026-09-18T09:38:19
  kind: decision
  summary: "细化:5组独立开关(平翘舌/n_l/r系/h_f/前后鼻音),部分开启时合成 per-combination schema"
  source: "2026-09-18 模糊音细化"
  affects: [fuzzy-pinyin-toggle]

- time: 2026-09-29T02:26:11
  kind: evidence
  summary: "仍未解决（发布不受阻，但 master 的 Build and Test 因此长期红）：CI runner 上按开关组合合成的 luna_pinyin_simp_fuzzy_<sig> schema 在 90 秒有界等待内始终没有部署出 prism，n_l-only / zh_z-only / subset 三条都停在「generated prism deployed」这道门上；本地同场景 19/19 全过。有界等待本身是对的（不再把部署没完成伪装成模糊音坏了），缺的是 runner 侧根因：可能是 rime_deployer 在容器里的后台部署线程根本没跑完，或 build/ 目录权限/挂载差异。下次动它先抓 runner 上 <user_dir>/build 的目录列表与 librime 部署日志，而不是再加等待时间。"
  source: "v1.1.4 发布会话"
  affects: [fuzzy-pinyin-toggle]

- time: 2026-09-30T03:34:08
  kind: reversal
  summary: "推翻 'prism 是编译慢/后台还在跑' 的假设（d7c20bb 据此加 join 并把 ready 门提到 120s）：librime 的 deploy_schema 走 RunTask，在调用线程上同步跑完 SchemaUpdate，那 240s 里没有任何编译在进行，deploy 早已失败返回且不留痕迹（返回值被丢弃 + ENABLE_LOGGING=OFF 把 librime 的 LOG(ERROR) 编译掉）。已在 4b9db4c 记录返回值、显式 close() 写文件、失败时 dump rime 用户目录+日志；下一次 CI 跑完即可定位到 SchemaUpdate::Run 的具体失败步。"
  source: "2026-09-30 CI flaky 排查（读 CI 日志 + librime 源码）"
  affects: [fuzzy-pinyin-toggle, e2e-harness-contract]

- time: 2026-09-30T03:51:42
  kind: reversal
  summary: "根因确认并已修复：ensure_fuzzy_schema 写完生成 schema 后没有 close()，std::ofstream 的缓冲未落盘，librime 紧接着同步读的正是这个文件 —— 读到空的 schema，SchemaUpdate::Run 在 config->LoadFromFile 失败处返回 false，deploy_schema 返回 false，prism 从未生成，而返回值一直被丢弃，于是表现成「等 240s 也没等到」。本地必绿是因为本机 libstdc++(__GLIBCXX__=20230528/gcc 11.4) 对 >=1024 字节的单次 << 会提前落盘，CI runner 的工具链不是这个阈值 —— 典型的「本机绿、runner 红」。join_maintenance_thread 已证明是 no-op（std::async 的 future 在第一次 get 后 valid()=false），不是原因。修复后 n_l-only 从等满 240s 变成约 2.6s 通过。"
  source: "2026-09-30 CI 36664971087 全绿（5/5 job），本地复现根因"
  affects: [fuzzy-pinyin-toggle]

- time: 2026-09-30T03:53:27
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [fuzzy-pinyin-toggle]
