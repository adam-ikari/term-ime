---
id: librime-standalone-portability
title: "librime fork 独立可移植性：两处真实缺陷"
category: decision
status: active
tags: [librime, portability, boost, glog, regex]
created: "2026-10-05T01:15:33"
updated: "2026-10-09T03:29:46"
---

<!-- compiled_truth -->
**结论（2026-10-09 更新）**：本页标题写的「两处真实缺陷」已过时 —— 现在是**四处**
（缺陷 3、4 于 2026-10-09 记录并修复于 `d46e4bee`/`1e289852`）。缺陷 1、2 修在
`c86f3a11`（tag `v1.1.8-rime-stack`）。**判据不变**：「在 term-ime 里绿」推不出
「fork 可独立移植」，必须脱离 term-ime 单独构建 + 跑 fork 自己的测试。

而且 2026-10-09 的独立验证给出一个**尚未关闭**的坏消息：`make deps` 在 python-free
PATH 下全过，但 fork 自己的 `make test` 在独立克隆里**过不了**（见末节，与 opencc
无关）。这条判据目前只做到「依赖构建独立」，没做到「测试套件独立」。

## 缺陷 1：glog >= 0.5 编译不过（显性）

`google::IsGoogleLoggingInitialized()` 的**声明**在 glog 0.5.0 被删干净了（不是移到
internal 头，所以没有版本宏可判）。符号仍在 `.so` 里 —— 这正是报错是
「is not a member of 'google'」而不是链接错误的原因。`nm -DC` 可验证。

修法：CMake `check_cxx_symbol_exists` + `RIME_HAVE_GLOG_IS_INITIALIZED` 条件编译。
**探测的是「能不能编译」，所以判的是声明在不在** —— 这是关键，若改成查库符号会
误判为真。

**这个缺陷一直隐形的原因**：term-ime 走 vendored glog submodule，恰好有那个符号。
只有脱离 term-ime、用发行版 glog 才会撞到。

## 缺陷 2：drop Boost 换 std::regex 悄悄改坏 algebra（隐性，更严重）

boost::regex 是 PCRE 语义，std::regex 是 ECMAScript，对简写类的处理不同而且
**不报错**：

| 模式 | 输入 | PCRE 应当 | std::regex 实际 |
|---|---|---|---|
| `^(\l+)\d$` | `zhang1` | 匹配 | **不匹配**（`\l` = 字面字母 l） |
| `^(\l)\d$` | `l1` | 不匹配 | **匹配**（读反了） |
| `^(\u)\d$` | `Z1` | 匹配 | **抛 std::regex_error** |

后果：任何用了 `\l`/`\u` 的 `xform/`/`derive/`/`abbrev/`/`erase` 规则移植过来就坏，
且无任何提示。librime 自带测试 `RimeAlgebraTest.Projection` 一直在失败就是它。

修法：`TranslatePcreClasses()` 在 8 处 `pattern_.assign()` 统一翻译。
两个易错点：**字符类内必须翻译成裸区间**（`[a\l]` → `[aa-z]`；写成 `[a-z]` 会变成
嵌套的 `[[a-z]]` 而非并集）；`\\l`（转义反斜杠 + l）不能当简写类动。
`\d \D \w \W \s \S` 不翻译（ECMAScript 定义相同）；`\p{...}` 不翻译（ECMA 无
Unicode 属性支持，悄悄匹配别的东西比报错更糟）。

**这个缺陷也一直隐形的原因**：term-ime 自己的 107 条 algebra 规则里含 `\l`/`\u`
的是 **0 条**（全用 `([zcs])` 这类 POSIX 写法）。所以 term-ime 的 e2e 永远绿，
而移植别人的 schema 就会坏。

## 缺陷 3：opencc 词库生成挂在引擎的 configure/build 图上（2026-10-09）

引擎根本不需要那份 `.ocd2` 数据（运行时从 rime 共享目录读，见缺陷 4），但
**构建它**却把词库生成工具链绑进了引擎编译图：

- `deps/opencc/CMakeLists.txt:225` 无条件 `add_subdirectory(data)`；
- `data/CMakeLists.txt:1` `find_package(PythonInterp REQUIRED)` —— **configure 期**就
  要 host Python 解释器，Windows/NDK 上直接炸；
- `data/CMakeLists.txt:3` 要一个 host 工具 `opencc_dict`，`:39-44` 的 `Dictionaries`
  是 **`ALL` 目标**，`:132-148` 的 custom command 在构建期**执行**它 —— 交叉编译下
  换 target 也躲不开，报 `Exec format error`。

**term-ime 曾为它变通**：`term-ime/CMakeLists.txt:119-123` 把 host PATH 塞进构建步
（注释写着「dictionary generation happens during install」）—— 那是症状不是设计。
更糟的是 term-ime 传的 `-DOPENCC_BUILD_TOOLS=OFF` 是**空转 flag**：opencc 的真实
option 里没有这一项（`add_subdirectory(tools)` 在 `src/CMakeLists.txt:208` 无条件）。

修法：opencc vendored 进 fork（去 submodule 身份），加
`option(OPENCC_BUILD_DATA)`/`option(OPENCC_BUILD_TOOLS)` 罩住那两个 `add_subdirectory`，
DATA 依赖 TOOLS 时 configure 期 FATAL_ERROR；两个 option **默认 ON**（不改变上游行为），
消费方（fork `deps.mk`/`build.bat`、term-ime `CMakeLists.txt`）传 OFF。删掉 PATH 变通。

## 缺陷 4：`PKGDATADIR` 烧死构建机绝对路径，是港/臺数据的唯一解析路径（2026-10-09，已发布缺陷）

**发布产物上港/臺字形切换静默失效**，且这是本次调查中最接近「已 ship 的 bug」的一条：

- schema 注册了 `simplifier@zh_hant_hk` / `simplifier@zh_hant_tw`
  （`data/rime-data/luna_pinyin.schema.yaml:63-65`，且在 `switches` 里用户可达）；
- 但 `dict/opencc/` 只有 `t2s.json`/`t2s_full.json`/`TSCharacters.ocd2`/
  `TSPhrases.ocd2`/`variants*.txt`，**没有** `t2hk.json`/`t2tw.json`；
- 那 4 个文件（加 `HKVariants.ocd2`/`TWVariants.ocd2`）只存在于
  `build/_deps_stage/share/opencc/`，靠 `libopencc.a` 里编译期烧进去的绝对路径
  `PKGDATADIR`（`deps/opencc/CMakeLists.txt` 的 `add_definitions(-DPKGDATADIR=...)`）
  被 `Config.cpp:183-193` 兜底找到。`strings` 实测该 `.a` 里有
  `/home/gem/project/term-ime/build/_deps_stage/share/opencc/`；
- 离开构建机，`simplifier.cc:337-339` catch 后返回 `nullptr`，`SimplifierComponent::Create`
  失败 → **filter 静默不存在**，不报错、不降级提示。

修法是补齐闭包（4 文件、约 10 KB，闭包很小：所有 schema 只引用 `t2s_full.json`/
`t2hk.json`/`t2tw.json`，后两者各自只引用 `HKVariants.ocd2`/`TWVariants.ocd2`）+
fork configure 期遍历 `dict/opencc/*.json` 的引用做 FATAL_ERROR 断言 + term-ime 侧
gtest 扫 schema 的 `opencc_config:` 值断言都在 `build/share/rime-data/opencc/` 里。

**反向对照必须做**：`unshare -rm` 把 `build/_deps_stage/share/opencc` bind 成空目录，
再驱动港/臺切换 —— 修复前该场景必须失败、修复后必须通过。只验正面会放过静默失败
（同 `rime-data-dir-discovery` 的教训）。现在 `PKGDATADIR` 不再是运行时输入。

## 独立构建的另一组阻塞（与 opencc 无关，2026-10-09 观察，仍未关闭）

在 `/tmp` 递归克隆 fork、PATH 掐掉 python 后：

- `make deps` **全过**（`CMakeCache` 里 `PYTHON_EXECUTABLE` 0 处、prefix 下无
  `share/opencc`、零 `.ocd2` 生成）—— 缺陷 3 的修复在独立环境成立。
- `make test` **过不了**，两处与 opencc 无关的既有问题：
  1. leveldb 静态库不带 `-fPIC`，链接 shared `librime.so` 时失败；
  2. 加 PIC 重编后，`rime_api_console` 链接报 `undefined reference to _ULx86_64_step`
     （glog 找 libunwind / frame-pointers 的既有搭配问题）。

这两条是 fork 独立测试路径的历史欠账，本轮**只记录不谎报**：它们不是 opencc 解耦
引入的，但它们是「fork 独立可移植」这条判据目前真正的下限。

## 教训：vendored 依赖会掩盖移植缺陷（缺陷 3、4 同源）

四处缺陷的成因相同：**term-ime 的构建路径恰好提供了 fork 独立构建时不存在的东西**
—— vendored glog 有那个声明；term-ime 的规则不用 `\l`；构建机上有 python 且
`_deps_stage/share/opencc` 恰好可被 `PKGDATADIR` 命中。

缺陷 4 还要加一条：**「链接了库」不等于「数据在运行时可解析」**。librime 只
link libopencc，数据闭包靠一个烧死的绝对路径兜底，在开发者机器上永远绿。

所以每次改 fork 都要两边都验：term-ime 的 gtest + 7 套 py e2e，以及 fork 自己的
`ctest`；而涉及运行时数据解析的，还要加**反向对照**（把兜底路径抹掉再测）。

## 写这类测试的一个坑

我给 `TranslatePcreClasses` 写的回归测试里有两条**期望值是错的**，第一次跑就失败：

- 拿 `llll1` 当「错误读法的判别例」—— 但 `llll` 本身就是四个小写字母，
  **正确语义也匹配**，无法判别。真正的判别是「`zhang1` 能不能匹配」。
- 拿 `b9` 当「不在 `[a-l]` 里」—— 但 `b` **就是**小写字母，应该匹配。
  真正的非匹配是大写。

两次都是「想当然的判别例其实不判别」。断言失败时先怀疑自己的期望值，
而不是先怀疑刚写完的修复。


## Timeline

- time: 2026-10-05T01:15:33
  kind: decision
  summary: "Created this page: librime fork 独立可移植性：两处真实缺陷"
  source: created via brain create-page
  affects: [librime-standalone-portability]

- time: 2026-10-05T01:15:33
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [librime-standalone-portability]

- time: 2026-10-09T03:29:46
  kind: decision
  summary: "新增缺陷 3（opencc 词库生成挂引擎 configure/build 图）与缺陷 4（PKGDATADIR 烧死构建机绝对路径是港/臺唯一解析路径 → 发布产物静默失效）；记录独立 make test 的 leveldb-PIC / glog-unwind 未关闭阻塞；教训段归并同源"
  source: "2026-10-09 opencc 解耦（fork d46e4bee / 1e289852）"
  affects: [librime-standalone-portability]
