---
sidebar_position: 10
title: 许可证与分发
---

# 许可证与分发

term-ime 的源码以 **MIT License**（`Copyright (c) 2026 adam-ikari`）发布。但发布产物不是单一授权：静态二进制把多个第三方库编进去了，随包分发的拼音数据里还有 LGPL-3.0 与 Apache-2.0 的派生文件。这一页把每样东西的授权和分发形态讲清楚。

## 静态二进制里编了什么

`ti` 是一个完全静态链接的单文件，零运行时共享库依赖。下面这些第三方库的代码被**静态链接进了二进制**，再分发二进制时必须随附它们的版权声明与许可文本：

| 组件 | 版本 | 许可 | 版权方 |
|------|------|------|--------|
| librime（fork `librime-stl`） | v1.1.8-rime-stack | BSD-3-Clause | RIME Developers |
| OpenCC | 1.1.9 | Apache-2.0 | BYVoid/OpenCC |
| └ bundled marisa | 0.2.6 | BSD-2-Clause **或** LGPL-2.1-or-later（双授权，本项目取 BSD-2） | Susumu Yata |
| └ rapidjson（头文件） | 1.1.0 | MIT | THL A29 Limited / Milo Yip |
| └ darts-clone（头文件） | 0.32 | BSD-2-Clause | Susumu Yata |
| utf8proc | 2.11.3 | MIT | Steven G. Johnson 等 |
| FTXUI | 6.1.9 | MIT | Arthur Sonzogni |
| spdlog | 1.15.0 | MIT | Gabi Melman |
| nlohmann/json | 3.11.3 | MIT | Niels Lohmann |
| libuv | 1.52 | MIT | libuv project contributors |
| Boost.SML | 1.1.13 | Boost Software License 1.0 | Kris Jusiak |
| utf8-cpp（`utf8.h`） | — | Boost Software License 1.0 | Nemanja Trifunovic |
| X11 keysymdef.h | — | MIT/X11（Open Group） | The Open Group |
| yaml-cpp | 0.8.0 | MIT | Jesse Beder |
| leveldb | 1.23 | BSD-3-Clause | The LevelDB Authors |

**没有编进二进制的**（只在源码树或测试里，不随发布产物分发）：glog（构建时 `ENABLE_LOGGING=OFF`，librime 走 `no_logging.h`）、googletest（仅测试）、tclap / pybind11 / google-benchmark（OpenCC 工具链，`OPENCC_BUILD_TOOLS=OFF`/`OPENCC_BUILD_DATA=OFF`）。

:::note
实际链接进二进制的是 OpenCC 自带的 **marisa 0.2.6**（`deps/librime/deps/opencc/deps/marisa-0.2.6`），不是 `deps/librime/deps/marisa-trie`（0.3.1）——后者虽被构建，但其 stage 产物被 OpenCC 的 install 覆盖了。两者都是 Susumu Yata 的双授权（BSD-2 / LGPL-2.1+），本项目按 BSD-2-Clause 分发。
:::

## 随包分发的拼音数据

`share/term-ime/rime-data/` 里的数据授权是**按文件分开的**，不能整体当作 MIT。词库本体（`luna_pinyin.dict.yaml`、`essay.txt`、`opencc/`）的授权详见 [`deps/librime/dict/LICENSE`](https://github.com/adam-ikari/librime-stl/blob/master/dict/LICENSE)：

| 文件 | 授权 | 来源 |
|------|------|------|
| `luna_pinyin.dict.yaml` | MIT | 本项目原创（繁体底本转简体去重） |
| `opencc/t2s_full.json`、`opencc/variants*.txt` | MIT | 本项目原创 |
| `essay.txt` | **LGPL-3.0** | 派生自 [rime/rime-essay](https://github.com/rime/rime-essay) |
| `opencc/t2s.json`、`t2hk.json`、`t2tw.json`、`*.ocd2` | **Apache-2.0** | 派生自 [BYVoid/OpenCC](https://github.com/BYVoid/OpenCC) |

term-ime 自有的方案文件（`data/rime-data/*.yaml`，随包一起分发）：

| 文件 | 授权 | 来源 |
|------|------|------|
| `pinyin.yaml` | **LGPL-3.0** | [rime/rime-luna-pinyin](https://github.com/rime/rime-luna-pinyin) 逐字副本 |
| `luna_pinyin.schema.yaml` | **LGPL-3.0** | rime-luna-pinyin 派生（改 1 行 `opencc_config`） |
| `luna_pinyin_simp.schema.yaml` | **LGPL-3.0** | rime-luna-pinyin 派生（重写） |
| `luna_pinyin_simp_fuzzy.schema.yaml` | MIT | 本项目原创 |
| `default.yaml` | **BSD-3-Clause** | librime `data/minimal/default.yaml` 派生 |

## 分发渠道

### GitHub Releases（主要）

推送 `v*` tag 触发 [`release.yml`](https://github.com/adam-ikari/term-ime/blob/master/.github/workflows/release.yml)，产出两个 Linux tarball：

```
term-ime-linux-x86_64.tar.gz     (+ .sha256)
term-ime-linux-aarch64.tar.gz    (+ .sha256)
```

解包后的目录形态（即安装形态）：

```
term-ime/
├── bin/
│   ├── ti                 # 主命令
│   └── term-ime -> ti     # 兼容别名
├── share/term-ime/
│   ├── rime-data/         # 方案 + 词库 + essay + opencc 数据（含 LICENSE / LICENSE-schemas.txt 逐文件授权）
│   └── translations/      # UI 翻译（en / zh-CN）
├── LICENSES/             # 各第三方许可全文（19 个文件）
├── LICENSE                # 本项目 MIT
└── README.md
```

`LICENSES/` 由 [`tools/collect-licenses.sh`](https://github.com/adam-ikari/term-ime/blob/master/tools/collect-licenses.sh) 从 vendored 依赖组装，release 流程在打包前重跑一次，保证文本与当次链接的版本一致。逐项对应关系：

| tarball 内文件 | 覆盖组件 |
|----------------|----------|
| `librime-BSD-3-Clause.txt` | librime |
| `opencc-Apache-2.0.txt` | OpenCC |
| `marisa-BSD-2-Clause.txt` | bundled marisa 0.2.6 |
| `darts-clone-BSD-2-Clause.txt` | darts-clone |
| `rapidjson-MIT.txt` | rapidjson（从 header 内嵌声明切出） |
| `utf8proc-MIT.txt` / `ftxui-MIT.txt` / `spdlog-MIT.txt` / `nlohmann-json-MIT.txt` / `yaml-cpp-MIT.txt` | 同名库 |
| `libuv-MIT.txt` / `libuv-extra.txt` | libuv 及其附加声明 |
| `boost-sml-BSL-1.0.txt` | Boost.SML |
| `utf8-cpp-BSL-1.0.txt` | utf8-cpp（从 `utf8.h` 切出） |
| `X11-keysymdef.txt` | `keysymdef.h`（从 header 切出） |
| `leveldb-BSD-3-Clause.txt` | leveldb |
| `LGPL-3.0.txt` | `essay.txt`、`pinyin.yaml`、`luna_pinyin*.schema.yaml` |
| `GPL-3.0.txt` | LGPL-3.0 的引用基础文本 |
| `rime-dict-data.txt` | rime-data 目录的逐文件授权声明（同时随 `rime-data/LICENSE` 落盘）；方案文件的授权为 `rime-data/LICENSE-schemas.txt`，两者来源不同各自成文 |

`LICENSES/` 中无独立许可文件的三项（rapidjson、utf8-cpp、X11 keysymdef）由脚本按行号从对应 header 切出，并在切完后校验内容包含预期标记；行号漂移时脚本直接失败，而不是发出截断的许可。

每个 tarball 附 `.sha256` sidecar，三方校验：GitHub API digest / sidecar / 独立 `curl` 复算。

### 一键安装脚本

```bash
curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
```

`install.sh` 从 GitHub Releases 拉 tarball、校验 sha256、装到 `PREFIX`（默认 `~/.local`，免 sudo；`--prefix /usr/local` 走 sudo）。装的内容就是上面那个目录形态。

### 源码构建

```bash
git clone --recursive https://github.com/adam-ikari/term-ime.git
cd term-ime
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

所有第三方依赖都以 git submodule 形式随源码 vendored，克隆即有，无需系统装 `-dev` 包。

### 已停用的渠道

Homebrew tap 在 **v1.1.6** 起停用。Termux/Android 在 2026-10-02 彻底删除（实验性支持期间未在真机验证）。

## 再分发须知

再分发 term-ime 的二进制或数据时，必须满足各组件的授权义务：

- **MIT / BSD / BSL / Boost**：随二进制附版权声明与许可文本。
- **Apache-2.0**（OpenCC 库与数据）：附 Apache-2.0 全文；若上游有 `NOTICE`，一并附上。
- **LGPL-3.0**（`essay.txt`、`pinyin.yaml`、`luna_pinyin*.schema.yaml`）：附 LGPL-3.0 全文，并提供对应源码（数据文件本身即源码，随包已提供；修改过的地方——`essay.txt` 转简体去重、schema 改 `opencc_config`——需标注）。

三条分发路径都随附许可文本：release tarball 根目录下是 `LICENSE`（本项目 MIT）与 `LICENSES/`（19 个第三方许可全文，由 `tools/collect-licenses.sh` 在打包前从 vendored 依赖重新生成）；`install.sh` 会把它们装到 `$PREFIX/share/term-ime/`（`cmake --install` 同理）。`rime-data/` 自身带两份声明——`LICENSE`（词库，来自 `deps/librime/dict/`）与 `LICENSE-schemas.txt`（方案，来自本仓库 `data/rime-data/`）——随应用一起落盘，而不是只在解包瞬间可见。

完整源码（含所有 submodule 的许可文件）在 [github.com/adam-ikari/term-ime](https://github.com/adam-ikari/term-ime) 与 [github.com/adam-ikari/librime-stl](https://github.com/adam-ikari/librime-stl)。
