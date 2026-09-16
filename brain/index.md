# Brain Index

_Auto-generated. Last updated 2026-09-16T05:03:10.611Z._

- [candidate-bar-contract](pages/candidate-bar-contract.md) — category: decision | tags: [ui, candidates, rime, config] | ## 约束
- [config-load-sanitization](pages/config-load-sanitization.md) — category: decision | tags: [config, robustness, rime] | <current best understanding — replace this with the real content>
- [config-save-never-throws](pages/config-save-never-throws.md) — category: decision | tags: [config, error-handling, libuv] | `AppConfig::save` **不抛异常**，返回 `bool` 表示成功与否（写失败、目录不可写、序列化失败都走 `false`）。
- [e2e-harness-contract](pages/e2e-harness-contract.md) — category: concept | tags: [testing, pty, harness] | ## 约束（违反即脆性测试）
- [event-loop-libuv-handle-ownership](pages/event-loop-libuv-handle-ownership.md) — category: decision | tags: [libuv, event-loop, memory-lifetime] | EventLoop 持有的 libuv 句柄（stdin/PTY 的 uv_poll/uv_stream 等）在 `uv_close` 调用时**立即把所有权 release 给 libuv**：close 回调是唯一合法的 `delete` 点。
- [fuzzy-pinyin-toggle](pages/fuzzy-pinyin-toggle.md) — category: decision | tags: [rime, config, settings, schema] | ## 模糊音组的落点（2026-09-16）
- [opencc-chain-for-simplified](pages/opencc-chain-for-simplified.md) — category: decision | tags: [rime, opencc, i18n, schema] | ## variants_ext.txt（2026-09-16）
- [parser-stream-state-contract](pages/parser-stream-state-contract.md) — category: concept | tags: [parser, utf8, csi, osc] | `Parser` 的输入是**任意切分的字节流**——PTY 的 read 边界与转义序列边界无关。
- [startup-readiness-window](pages/startup-readiness-window.md) — category: decision | tags: [rime, startup, pty, testing] | ## 已观测事实
- [status-bar-owns-last-row](pages/status-bar-owns-last-row.md) — category: decision | tags: [terminal, layout, resize, renderer] | 状态栏（Status bar）固定渲染在终端**最后一行**，属于 UI 保留区，不属于 shell/PTY 的绘制区。
