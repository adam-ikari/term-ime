---
id: librime-standalone-portability
title: "librime fork 独立可移植性：两处真实缺陷"
category: decision
status: active
tags: [librime, portability, boost, glog, regex]
created: "2026-10-05T01:15:33"
updated: "2026-10-05T01:15:33"
---

<!-- compiled_truth -->
**结论（2026-10-05）**：fork 曾有两处真实缺陷使「脱离 term-ime 独立构建/移植」不成立，
均已修复（`c86f3a11`，tag `v1.1.8-rime-stack`）。现在 `git clone` fork +
自带 CMake + `-DBUILD_TESTS=ON` 可 100% 通过。

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

## 教训：vendored 依赖会掩盖移植缺陷

两处缺陷的成因相同：**term-ime 的构建路径恰好提供了 fork 独立构建时不存在的东西**
（vendored glog 有那个声明；term-ime 的规则不用 `\l`）。

所以「在 term-ime 里绿」不能推出「fork 可独立移植」。判据只能是
**脱离 term-ime 单独构建 + 跑 fork 自己的测试套件**。

这也是为什么每次改 fork 都要两边都验：term-ime 的 118 gtest + 7 套 py e2e，
以及 fork 自己的 `ctest`。前者证明没弄坏宿主，后者证明还立得住。

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
