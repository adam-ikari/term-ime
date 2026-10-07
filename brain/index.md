# Brain Index

_Auto-generated. Last updated 2026-10-07T07:34:00.248Z._

- [candidate-bar-contract](pages/candidate-bar-contract.md) — category: decision | tags: [ui, candidates, rime, config] | ## 约束
- [config-load-sanitization](pages/config-load-sanitization.md) — category: decision | tags: [config, robustness, rime] | 配置加载侧消毒原则：越界钳制、坏类型按缺省（不丢弃整份文件）。
- [config-runtime-binding](pages/config-runtime-binding.md) — category: decision | tags: [config, shell, logging, rime] | 配置字段只有被运行时读到才算接线。
- [config-save-never-throws](pages/config-save-never-throws.md) — category: decision | tags: [config, error-handling, libuv] | `AppConfig::save` **不抛异常**，返回 `bool` 表示成功与否（写失败、目录不可写、序列化失败都走 `false`）。
- [cpp-standard-floor](pages/cpp-standard-floor.md) — category: decision | tags: [librime, build, cpp-standard, constraint] | librime fork（`v1.1.8-rime-stack`）以 C++17 为地板，不可降级到 C++11。
- [dictionary-repo-split](pages/dictionary-repo-split.md) — category: decision | tags: [dict, rime, release] | ## 词库已并入 librime fork 的 dict/（submodule 9→8，rime 栈一个 tag）
- [e2e-harness-contract](pages/e2e-harness-contract.md) — category: concept | tags: [testing, pty, harness] | ## 这一页的判定标准（贯穿全部内容）
- [enter-does-not-commit-composition](pages/enter-does-not-commit-composition.md) — category: decision | tags: [ime, keyboard, input, bug] | ## 现象（2026-10-01 实测发现，Linux 与 Android 同样存在）
- [event-loop-libuv-handle-ownership](pages/event-loop-libuv-handle-ownership.md) — category: decision | tags: [libuv, event-loop, memory-lifetime] | EventLoop 持有的 libuv 句柄（stdin/PTY 的 uv_poll/uv_stream 等）在 `uv_close` 调用时**立即把所有权 release 给 libuv**：close 回调是唯一合法的 `delete` 点。
- [fuzzy-pinyin-toggle](pages/fuzzy-pinyin-toggle.md) — category: decision | tags: [rime, config, settings, schema] | ## 模糊音组的落点（2026-09-16）
- [i18n-resource-resolution](pages/i18n-resource-resolution.md) — category: decision | tags: [i18n, translation, install, packaging] | **结论**：翻译资源按固定顺序解析，第一个含 `zh-CN.json` 的目录胜出：
- [librime-standalone-portability](pages/librime-standalone-portability.md) — category: decision | tags: [librime, portability, boost, glog, regex] | **结论（2026-10-05）**：fork 曾有两处真实缺陷使「脱离 term-ime 独立构建/移植」不成立，
- [opencc-chain-for-simplified](pages/opencc-chain-for-simplified.md) — category: decision | tags: [rime, opencc, i18n, schema] | **opencc 变体字表 `variants_ext.txt` 的位置变了两次，现在在 librime fork 里。
- [parser-stream-state-contract](pages/parser-stream-state-contract.md) — category: concept | tags: [parser, utf8, csi, osc] | `Parser` 的输入是**任意切分的字节流**——PTY 的 read 边界与转义序列边界无关。
- [passthrough-query-architecture](pages/passthrough-query-architecture.md) — category: decision | tags: [terminal, parser, architecture, query] | ## 事实
- [pty-outbound-write-queue](pages/pty-outbound-write-queue.md) — category: decision | tags: [pty, libuv, write, ime] | > 页面标题里的"永不丢弃"已被推翻（见时间线 reversal），当前结论以下面为准。
- [release-publish-flow](pages/release-publish-flow.md) — category: concept | tags: [release, ci] | 发布 = 打 tag 触发 CI 出包，GitHub Release 就是全部发布产物。
- [review-fix-wide-char](pages/review-fix-wide-char.md) — category: decision | tags: [parser, renderer, review, wide-char] | 评审修复轮(4 commits):宽字符占右半格+参数化光标 CSI 上限+中文组合态透传控制字节+Pty::write 总上限。
- [rime-data-dir-discovery](pages/rime-data-dir-discovery.md) — category: decision | tags: [rime, packaging, install, release] | **结论（v1.1.4 起）**：`RimeIme::initialize()` 的 shared_data_dir 候选顺序，第一个"带标记"的目录胜出，与列表顺序无关；一个标记都没有时回落为第一个存在的目录（`select_shared_data_dir`，`src/ime/
- [rime-punctuator-preset](pages/rime-punctuator-preset.md) — category: decision | tags: [rime, 标点, punctuator, schema] | **结论**：schema 想用标点映射，必须在自己的 `.schema.yaml` 里写 `punctuator: { import_preset: default }`。
- [settings-panel-row-budget](pages/settings-panel-row-budget.md) — category: decision | tags: [ui, settings, i18n, ftxui] | **结论**：设置面板的内容高度是 **19 行**（7 项 + 1 分组标题 + 1 描述行 + 标题/分隔/提示/关闭），FTXUI 超预算只裁不折，先被裁的是最底下的「关闭」，面板打不开设置。
- [sgr-color-support](pages/sgr-color-support.md) — category: decision | tags: [terminal, parser, renderer, sgr] | <current best understanding — replace this with the real content>
- [startup-readiness-window](pages/startup-readiness-window.md) — category: decision | tags: [rime, startup, pty, testing] | ## 已观测事实
- [status-bar-owns-last-row](pages/status-bar-owns-last-row.md) — category: decision | tags: [terminal, layout, resize, renderer] | 状态栏（Status bar）固定渲染在终端**最后一行**，属于 UI 保留区，不属于 shell/PTY 的绘制区。
- [termux-android-target](pages/termux-android-target.md) — category: decision | tags: [termux, android, build, cmake] | ## 结论：Termux 支持已彻底删除（2026-10-02）
- [website-spread-redesign](pages/website-spread-redesign.md) — category: decision | tags: [website, 传播, 文案] | **决定**：官网落地页按“可传播”重做，范围 = 结构 + 文案 + 视觉全做；主战场是中文技术社区（V2EX/掘金/知乎/公众号）。
