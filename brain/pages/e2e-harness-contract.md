---
id: e2e-harness-contract
title: "测试有效性契约：e2e / gtest / fuzz（判定标准是观测量有无区分度）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-10-10T18:32:49"
---

<!-- compiled_truth -->
## 这一页的判定标准（贯穿全部内容）

**测试只有在「观测量在有 bug 和无 bug 时确实不同」时才有效。** 全绿本身不是证据 ——
一个永远为真的断言同样全绿。同义反复、空转断言、行为等价的变异，都是同一个错：
在没有区分度的观测上宣布通过。

已发生的八次空转断言，每一次的形态都不同，所以不能靠记 checklist 防住：
预测错字符（「」vs 实际「〔」）；比对两帧分别捕获的图像；`hits` 比 `segments` 短导致
`zip` 循环体不执行；`drain()` 把承载证据的输出丢掉；断言在半读完的一帧上；
probe 列表中途被清空退化成随机序列；`got` 为空导致 `zip` 空转通过；helper 的 `read()`
吃掉重绘。

## e2e 脚本必须满足的契约（2026-09-15 起，至今有效）

- hermetic 环境（独立 HOME），**就绪门**等 librime 部署完成再开测
- 固定 `SHELL=/bin/sh`，probe 式断言（**不得匹配提示符文本**）
- **不得写死开发机绝对路径**：二进制与工作目录由 `__file__` 推仓库根。
  `test_settings_panel_e2e.py` 曾写死 `/home/gem/project/term-ime`，让 ci.yml 的 e2e
  job 从接入那天起每次都在第一步 `ERROR: ... not found` 退出 —— 连续 4 次红与代码无关，
  而**本机永远跑不出**，这类缺陷只在 runner 上暴露
- 应用自身的失败路径必须在失败时留下证据（返回值 + 目录/日志 dump），因为 librime
  的日志在本项目构建里是关掉的
- 卡在有界等待上时，**等待窗口不是修复手段**：先确认那个操作是不是异步的。
  librime `deploy_schema` 是同步的，240s 全是白等

### 「长期 flaky」优先怀疑确定性失败

同一断言在多个 run 里以**完全相同的方式**失败 —— 那是 bug 的形状，不是竞态的形状。
连续 8 次红被贴上「已知 flaky 不阻塞发布」，实际是确定性失败。判据同前。

给未验证的东西在 CI 里建 job 时，要带**负例自检**（拿已知不合格的输入跑同一段校验，
必须失败），否则断言可能是空转的。

### CI 绿灯不等于依赖外部配额的服务可靠

install.sh 曾用 GitHub API 列 release 再逐个探测资产：每次安装 1 次 API + 最多 30 次
HEAD，而未认证限额仅 60 次/小时/IP，几个人同时装就耗尽，之后所有安装以裸 403 失败。
CI 绿灯是虚假信心（额度还没用完），直到把 rate_limit 打到 0/60 才暴露。

验证方式应选「**把依赖整个屏蔽掉仍能工作**」，而不是「看它成功了」—— 后者区分不了
「真的没依赖」和「额度还够」。同类：静默跳过失败（`set()` 遮蔽 `-D` 变量、
`std::ofstream` 未 close 导致 librime 读到空文件）都是被外部环境差异暴露的。

## fuzz：仓库里早有模型，只是没人驱动（2026-10-02）

`tests/monkey_sequences.py` 一直在建模动作空间 —— 字母、数字、Ctrl+A 组合、
**畸形 CSI**、方向键、**resize**、wait，外加四个定向探针
（`_p1_escapecsi` / `_p2_toggle_mid_composition` / `_p3_settings_esc` /
`_p6_exit_hang`）。但**没有任何东西驱动它**，且 `grep monkey_sequences ci.yml` 为空 ——
等于只活在文档中。

新增 `tests/fuzz_drive.py`：import 那个模型（不重新实现），检查不变量是
**进程必须活着**。变异验证：`BIN` 指向 `exit 3` → `exited at step 0 (code 3)`；
指向 `kill -SEGV` → `exited at step 0 (signal 11)`。

### 这个驱动抓不到什么 —— 必须写清楚，否则会被当成「跑过了所以没问题」

- **不检查输出对错**。存活不代表字节到达了 shell；行为不变量在具名 e2e 里
- **不检查 hang**。卡死但没退出的进程读作 ok（用 `sleep 600` 替身验证过，确实 ok）
- 每轮结束就 kill，所以关机崩溃、空闲后首个按键崩溃看不见

不进 CI（一轮约 6s librime 部署 + 每步 20ms），属手动工具。跑过量：45 轮随机
（约 5000 动作）+ 8 轮定向探针全干净 —— **结论是「没找到崩溃」，不是「没有问题」**。

**故意没加 liveness 探针**：那 10 行的收益只在手动跑 fuzz 时兑现，而仓库里所有循环
本身都有界。作为已知局限记录，不假装覆盖了。

## 存量：同义反复的测试（2026-10-02）

扫「源文件被多少测试引用」时发现 `tests/test_ime_state.cpp` **整个文件**都是同义反复：

    EXPECT_NE(ImeState::Inactive, ImeState::Composing);   // 断言两个枚举值不同
    cand.text = U"你好"; EXPECT_EQ(cand.text, U"你好");   // 断言自己等于自己

测的是 C++ 语言，不是 term-ime —— **无论代码好坏都不可能失败**。已删除。

危害不在那 44 行，而在于它撑起「114 个测试」这个数字，让人以为 IME 状态被覆盖了。
这与新增里避免空转是同一类错误的**存量版**：一直在新增里避免，却没清过旧的。

全仓库扫过一遍（正则匹配 `EXPECT_NE(\w+::\w+,\s*\w+::\w+)` 与「赋值后立即断言自己」
两种模式），只有这一个文件命中。

## tests/test_ime_contract.cpp：8 条真实契约（2026-10-02）

`ImeEngine` 被文档宣传为可嵌入，接口保证是公开承诺，但此前无任何东西断言它。
契约一改，要么表现为某条不相关的屏幕断言失败，要么根本不响。

| 变异 | 被谁抓到 |
|---|---|
| `input()` 在英文模式也接受 | InputOutsideACompositionIsRefused |
| `select()` 不排空 commit | SelectCommitsAndReportsTheText |
| `cancel()` 变空操作 | CancelClearsBufferAndReturnsToInactive |

第一条最关键：**app 的按键分发正是靠 `input()` 的返回值区分「键不是我的，要转发给
shell」**。它一旦返回 true，按键就会被静默吞掉 —— 与本轮修的三个吞键 bug 同一根源。

### 变异必须真的改变行为

第一次变异「删掉 `cancel()` 里的 `clear_composition`」**通过了**。查下来是行为等价的
变异：它前面已经发了 `XK_Escape`，组合态早被清掉，删掉的是冗余保险。
**行为等价的变异上「测试通过」什么也没证明。** 换成把整个 `cancel()` 改成空操作。

### 测试可以纠正假设

最初断言「`select()` 之后能用 `take_commit()` 取回同样内容」，测试报失败。
实际 `select()` 内部已排空，所以**取不到才是对的** —— 那正是防二次上屏的不变量。

成本：每个 fixture 都要 `initialize()` 部署词典（~3s），gtest 从 4s 涨到 32s。


## 一个存在但不在 CI 里的测试，等于没有测试（2026-10-02）

收尾扫残留时发现 `tests/test_paste_delivery.py` **从来没在 CI 里跑过** ——
`grep -rn test_paste_delivery .github/` 为空。

它测大段粘贴的**到达率与字节序**（`--stall` 让读者先睡，覆盖出站队列溢出），
本机连跑三次都是 7-8s 稳定通过。这正是「本地绿 + 守护力为零」的形态：
维护者以为覆盖了粘贴路径，实际上改坏粘贴不会有任何东西变红。已加入 ci.yml。

判据很简单：**`grep <套件名> .github/workflows/` 为空 = 这个测试不保护任何东西。**
新增套件时必须同时改 ci.yml，否则它只是本机的一个脚本。

## 文档里没写的测试，等于不存在的测试

`TESTING.md` 花了大量篇幅逐条列出 3 个 C++ e2e 二进制的用例名，而**完全没有提**
7 个 python e2e 套件 —— 那才是 CI 真正跑的一批，也是三个吞键 bug 唯一被逮住的地方。
同期它的 gtest 用例表写的是 6/6/4/8/13，实际值 48/12/8/11/23，并且列了一个已删除的
`test_ime_state.cpp`。

**测试文档会塑造「哪里有覆盖」的判断。** 照着它去找漏洞，会去查 UI 组件渲染，
而不会想到去查按键分发 —— 而按键分发恰恰是三个真实 bug 的所在地。
表格里的数字改为从 `--gtest_list_tests` 得出并标注「不要手工维护」；
fuzz 驱动的三条盲区（不检查输出对错 / 不检查 hang / 每轮 kill 所以看不见关机崩溃）
也一并写进去，避免被读成「跑过了所以没问题」。


## 死错误路径：`set -euo pipefail` 会让 `if [[ -z "$X" ]]` 检查形同虚设（2026-10-04）

发 v1.1.7 时真撞上 GitHub API 限流，用户看到的是一行裸报错：

    curl: (22) The requested URL returned error: 403

一开始以为是「提示太薄」，去加提示就行。**真因是那段提示根本到不了**：

    set -euo pipefail                        # 第 14 行
    VERSION="$(fetch "$API" | grep -m1 '"tag_name"' | sed ...)"
    if [[ -z "$VERSION" ]]; then
        echo "error: could not determine latest release"   # 死代码
        exit 1
    fi

`grep` 无匹配时退出 1 → `pipefail` 让非零冒泡进赋值 → `set -e` 当场杀掉脚本。
所以那段 `if` **恰好在它唯一该生效的场景里不可达**。

`find ... | head -1` 形式的取二进制检查同理：当 `find` 自己出错时也死
（`find` 对不存在的目录退出 1）。三处补 `|| true`。

### 判据：凡是有「失败时给用户看的话」的分支，就先问它可不可达

这是本会话遇到的**第三类**空转断言/死代码形态：

| 形态 | 症状 | 判据 |
|---|---|---|
| 同义反复断言 | 永远通过 | 观测量在有无 bug 时是否不同 |
| 行为等价的变异 | 变异通过 | 变异是否真的改变行为 |
| **死错误路径** | **报错信息永远不显示** | **失败时用户实际看到什么，实测一次** |

第三类最容易漏，因为它长得像「已处理错误」—— 代码在、消息写了、有 `exit 1`，
读起来很完整。唯一能发现的方式是**拿 stub 让它真的失败一次，看用户实际看到什么**。

### CI 没跑过的路径就是会烂

`install-test` 全程 `--version v9.9.9` 打本地 HTTP server，**版本解析那一步一次都
没执行过**。而它恰恰是决定用户拿到哪个版本的那一步。

新增一步：stub curl 让 API 返回 403（就是我真撞到的那次），断言的不只是「失败」，
而是三件事 —— 脚本自己的诊断分支确实执行了、消息点名了原因、消息给了逃生口。
变异验证：去掉 `|| true` 后以
`ERROR: the script own error branch never ran` 失败。

### 没改的部分也要说清楚

版本解析仍走 API。它是唯一事实来源，1 次/安装够用；真正的缺陷是失败时不可诊断，
这一点修好就够。为了「更稳」再加一层 tag 常量或 fallback，是把一个已修好的问题
重新复杂化。


## 文档声称的覆盖 ≠ 代码实际跑的覆盖（2026-10-05）

`tests/fuzz_drive.py` 的文档（brain 页面 + 我自己的 commit message）都写着它会跑
「四个定向探针」。**实际没有**：驱动只 import 了 `monkey_sequences`，然后用
`ms.weighted_action(rng, bias)` 生成随机序列，一次都没引用 `ms.PROBES`
（定义在 `monkey_sequences.py:340`，只在它自己的 `__main__` 里用过）。

所以那句「fuzz 驱动 + 4 个定向探针」是假的。这与之前记的空转断言同族，但形态
更新：**不是断言写错，而是文档把没跑的覆盖写成了跑了**。

已修：`run_probes()` 每次调用先跑完 `ms.PROBES`，再进随机轮次；汇总行分别报
「N/M probes clean, K/J random rounds clean」，这样「探针通过」这句话才对应探针
真的执行过。

### 判据

**任何「覆盖了 X」的陈述，都要能在代码里指出 X 在哪一行被执行。**
指不出来就当作没覆盖。这次是commit message 自己骗了自己 —— 而 commit message
恰恰是最容易把「打算做」写成「做了」的地方。

### 变异验证（顺带重新确认了那个已知盲点）

| 替身 | 探针结果 |
|---|---|
| `kill -SEGV` | 4/4 报 died |
| `exit 3` | 0/4 干净 |
| `sleep 600` | **4/4 干净** |

第三行是**故意留着不修的**：挂起但没退出的进程读作 ok。修它需要 liveness 探针，
而仓库里所有循环本身有界，所以只作为已知局限记录。这次是重新确认它仍然漏，
没有把它写成已覆盖。


## Timeline

- time: 2026-09-15T03:17:45
  kind: decision
  summary: "Created this page: python PTY 端到端脚本契约（tests/*_e2e.py）"
  source: "2026-09-15 e2e 脚本修复"
  affects: [e2e-harness-contract]

- time: 2026-09-15T03:17:45
  kind: decision
  summary: "python PTY 端到端脚本必须：hermetic 环境 + 固定 SHELL=/bin/sh + 就绪门 + 探针式断言（不得匹配提示符文本）"
  source: "2026-09-15 e2e 脚本修复"
  affects: [e2e-harness-contract]

- time: 2026-09-29T00:41:58
  kind: decision
  summary: "脚本不得写死开发机绝对路径：二进制路径与工作目录要由 __file__ 推仓库根（或统一用相对仓库根的 ./build/term-ime）。tests/test_settings_panel_e2e.py 里的 /home/gem/project/term-ime 字面量让 ci.yml 的 e2e job 从接入那天起每次都在第一步 \"ERROR: ... not found\" 退出，master 上连续 4 次红与代码无关 —— 这类缺陷只在 runner 上暴露，本机永远跑不出。"
  source: "2026-09-29 v1.1.3 发布"
  affects: [e2e-harness-contract]

- time: 2026-09-29T03:21:07
  kind: note
  summary: "设置面板的开/关指纹字串从 'Up/Down' 换成 'Esc/Tab'（三行按键提示并成一行后 Up/Down 不再出现）。断言面板是否还开着要靠这个字面量，改提示文案必须同步改 panel_gone。"
  source: "设置面板描述改造会话"
  affects: [e2e-harness-contract]

- time: 2026-09-29T09:59:18
  kind: decision
  summary: "test_fuzzy_pinyin 的 per-combination prism 部署在 CI 上长期失败（词库拆仓前 run 0e222cb 同样 10/13，非回归）：n_l-only/zh_z-only/subset 三个组合 240s 内等不到 prism.bin。90s→240s 未能修复，600s 反而撞 job 超时被 cancel（已回退 240s）。这三条是 pre-existing flaky，不阻塞发布；根因（CI 2 核编译 7 万词条词典的耗时 vs deploy 真不完成）待单独排查。"
  source: "2026-09-29 CI flaky 排查"
  affects: [e2e-harness-contract]

- time: 2026-09-30T03:34:17
  kind: decision
  summary: "e2e 断言卡在一个有界等待上时，等待窗口不是修复手段：先确认那个操作是不是异步的。librime deploy_schema 是同步的，240s 全是白等，把「编译慢」当根因会把一次静默失败伪装成 flaky（连续 8 个 run 因此长期红）。另外：应用自身的失败路径必须在失败时留下证据（返回值 + 目录/日志 dump），因为第三方库（librime）的日志在本项目构建里是关掉的。"
  source: "2026-09-30 CI flaky 排查"
  affects: [e2e-harness-contract]

- time: 2026-09-30T03:52:05
  kind: reversal
  summary: "撤回「这些 3 个组合是 pre-existing flaky、不阻塞发布」的结论：它不是 flaky，是确定性失败（同一批组合每次都红）。判据是同一断言在多个 run 里以完全相同的方式失败 —— 那是 bug 的形状，不是竞态的形状。下次再看到「长期 flaky」先怀疑确定性失败，并优先找「本地绿/runner 红」的工具链差异（缓冲阈值、文件落盘、locale），而不是加超时。"
  source: "2026-09-30 CI flaky 收尾"
  affects: [e2e-harness-contract]

- time: 2026-09-30T03:54:06
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-09-30T05:12:10
  kind: reversal
  summary: "撤回「这些都是 pre-existing flaky」式的跳过倾向的另一个实例：连续 8 个 run 红时，标签是「已知 flaky 不阻塞发布」，实际是确定性失败。判据同前——同一断言以完全相同方式重复失败即非竞态。CI 里给未验证的东西建 job 时，要带负例自检（拿已知不合格的输入跑同一段校验，必须失败），否则断言可能是空转的。"
  source: "2026-09-30 Termux 交叉编译"
  affects: [e2e-harness-contract]

- time: 2026-10-01T05:42:45
  kind: decision
  summary: "「自动发现」听起来比「写死常量」优雅，但当它依赖有配额上限的 API 时可靠性更差。install.sh 曾用 GitHub API 列出 release 再逐个探测 termux 资产：每次安装 1 次 API + 最多 30 次 HEAD，而未认证限额仅 60 次/小时/IP，几个人同时装就耗尽，之后所有手机安装以裸 403 失败。CI 绿灯是虚假信心（额度还没用完），直到真的把 rate_limit 打到 0/60 才暴露。改用已知 tag 常量，零请求。教训：CI 绿灯不等于依赖外部配额的服务可靠；验证方式应选「把依赖整个屏蔽掉仍能工作」而不是「看它成功了」——后者区分不了『真的没依赖』和『额度还够』。同类：自动跳过静默失败（本项目 opencc 用 set() 遮蔽 -D 变量、std::ofstream 未 close 导致 librime 读到空文件）都是被外部环境差异暴露的。"
  source: "2026-09-30/10-01 Termux 安装链路"
  affects: [e2e-harness-contract]

- time: 2026-10-01T10:47:01
  kind: decision
  summary: "写「连续三段拼音各自 Enter 提交」的测试时又写出一版空转断言，暴露两个新形态的坑。(1) **修复合并后 git checkout 不是回退**：Enter 修复早已 commit，用 git checkout src/core/app.cpp 拿到的是修复版，我据此以为「未修复版也通过」并差点得出错误结论；验证旧行为必须显式取父版本 。(2) **drain() 用局部 total 累加**：多次调用只保留最后一次的内容，于是只看到最后一段的提交，前两段的证据被丢掉，看起来像「只提交了一次」。多段测试必须用全局累加长度。修正后的判据：每段 Enter 提交后再补一次 Enter 把行首执行出来，正确版得到 3 条独立 not found（你好/世界/中国 各一条），坏版本得到 0 条 —— 明确可区分。另外发现一种隐蔽的空转：hits 比 segments 少时 zip 循环体根本不执行，逐段断言会全部真空通过；改用按索引取值并对缺失项显式失败。已把多段用例加进 test_fuzzy_pinyin.py，并用 30c0e2f^ 核实 4 条断言全部 FAIL。"
  source: "2026-10-01 多段提交测试：第三次踩空转断言"
  affects: [e2e-harness-contract]

- time: 2026-10-01T10:47:12
  kind: decision
  summary: "上一条 timeline 里的 'git show 30c0e2f^:src/core/app.cpp' 因为在 shell 里未加引号，尖括号被当成重定向，命令名丢失。完整写法：git show '30c0e2f^:src/core/app.cpp' > app_prefix.cpp（路径含冒号，必须整体加引号）。"
  source: "补记：上一条里命令名被 shell 吃掉了"
  affects: [e2e-harness-contract]

- time: 2026-10-01T12:04:21
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-01T13:58:59
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-01T17:00:35
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T00:13:25
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T02:02:12
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T11:18:57
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T11:23:01
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T11:51:47
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T11:52:10
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-04T22:59:52
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-04T23:00:18
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-05T02:27:23
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-05T02:27:38
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-10T17:22:26
  kind: evidence
  summary: "tests/test_paste_delivery.py 是唯一没固定 SHELL=/bin/sh 的 e2e 脚本，于是它测的是开发者登录 shell：本机 SHELL=/bin/zsh 时它稳定停在 MARKER 门（FAIL: keys never reached a live shell），改成 /bin/sh 后同一二进制全绿；CI runner 的 SHELL=/bin/bash 所以从未暴露。已补上 SHELL=/bin/sh。顺带留下的未决问题：zsh 下第一批键入到达提示符前被改写（观察到的 echo 变成 cho，屏幕上多出 ESC=c 与 ESC>），这可能是 App 与 zle 交互的真实缺陷，值得单独一轮排查，不要在 e2e 层用固定 SHELL 把它盖过去。"
  source: "2026-10-10 本机实测"
  affects: [e2e-harness-contract]

- time: 2026-10-10T18:32:49
  kind: evidence
  summary: "两条会让 e2e 断言失去判别力的陷阱（写 tests/test_wide_pair.py 时实测踩到）：(1) 断言用的 ASCII 标记若原样敲进命令行，shell 回显里就有它，strip_ansi 后的整帧匹配会「因为敲过」而成立——标记必须由 printf 的八进制转义生成（\\132=Z），让敲入的文本只含反斜杠数字；(2) parser 是把 shell 字节流重放到外层终端的，所以 Screen 影子网格的缺陷在原始输出里看不见，必须等一次 redraw_shell 才暴露——本项目里最省事的强制重绘是开设置面板（整屏 ESC[2J）再按 ESC 关闭。同一轮新增 tests/test_config_types_e2e.py（错类型键不再丢弃整个配置文件，判据是 max_candidates 仍生效 + 日志点名被忽略的键）。两个用例都验过判别力：test_wide_pair 对仅回退 screen.cpp 的二进制 1/4、对修复后 4/4；test_config_types_e2e 对仅回退 config.cpp 的二进制 0/2、对修复后 2/2。e2e 清单现为 10 项（tools/run-e2e.sh）。"
  source: "2026-10-11 补 e2e 缺口"
  affects: [e2e-harness-contract]
