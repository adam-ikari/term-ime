---
slug: roadmap
title: Roadmap
role: milestones
updated: "2026-09-17T16:25:32"
---

# Roadmap

Milestones are phases, not task lists. Each has one objective, a depends set,
and a done definition you can verify. Captured 2026-09 (replaces the 2026-06
scaffold). Core terminal + pinyin IME already shipped (M0); remaining phases
are real, code-grounded gaps. Priority split: **high = M1, M3**; **low = M2, M4**
(dropped 2026-09) — M3 shipped 2026-09, so the remaining high tier is M1.

```mermaid
gantt
  title term-ime Roadmap
 dateFormat YYYY-MM-DD
  section Core
  M0 terminal + pinyin IME :done, m0, 2026-06-22, 2026-09-14
  section High
  M1 config robustness :active, crit, m1, after m0, 7d
  M3 terminal emulation fidelity :done, m3, after m0, 14d
  section Low
  %% M2/M4 dropped 2026-09 — not scheduled
  M4 themes + kaomoji persistence (dropped) :m4, after m0, 10d
  M2 shuangpin schema (dropped) :m2, after m1, 14d
```

## M0 — Core terminal + pinyin IME (done)

Delivered, each backed by a decision page: PTY + VT/ANSI parser + grid renderer
([[parser-stream-state-contract]], [[status-bar-owns-last-row]]); full-pinyin
via librime with simplified-candidate conversion ([[opencc-chain-for-simplified]])
and fuzzy toggle ([[fuzzy-pinyin-toggle]]); adaptive candidate bar
([[candidate-bar-contract]]); settings panel with never-throw save
([[config-save-never-throws]]); cold-start readiness window
([[startup-readiness-window]]); libuv handle ownership
([[event-loop-libuv-handle-ownership]]); fully static single-file binary.

## Phases

| milestone | priority | objective | depends |
|---|---|---|---|
| M1 config robustness | **high** | Reject/clamp out-of-range config on load instead of trusting it; today `max_candidates` (and siblings) flow straight from JSON with no bound check (`src/core/config.cpp`). Define one validation pass + policy. | — |
| M3 terminal emulation fidelity (done) | **high** | (done) Close remaining VT/ANSI gaps (README: "更多转义序列") — SGR color queries, OSC passthrough — behind the parser stream contract. | — |
| M4 themes + kaomoji persistence | low | (dropped) Ship theme switching (README: "主题切换") and make kaomoji load/save from JSON instead of the hardcoded array (`src/ime/kaomoji.cpp:187,197`). | — |
| M2 shuangpin schema | low | (dropped) Add a second input method (双拼) alongside full-pinyin, selectable in settings. Needs a new `data/rime-data/*.schema.yaml` + key-map; only luna_pinyin* exist today. | M1 |

## Done means

- M1: `term-ime.json` with `max_candidates: 20` loads clamped to a legal value;
  a regression test feeds out-of-range keys and asserts the sanitized config.
- M3: verified 2026-09 — 16-color SGR + ED/EL, 256-color + truecolor; SGR/DSR/OSC
  queries round-trip via the real terminal ([[passthrough-query-architecture]]);
  golden parser test reproduces each previously-mishandled sequence, grid matches
  a reference terminal.
- M4: switching theme re-renders status/candidate bar live (no restart);
  kaomoji edits survive a restart via JSON.
- M2: 双拼 selectable in the settings panel; typing a 双拼 code yields candidates;
  e2e (`tests/*_e2e.py`) drives it.

## Sequencing

M0 shipped. **Do the high tier first: M1 and M3 are independent and
parallelizable.** M2 (双拼)、M4 (主题+颜文字)已于 2026-09 放弃,不再排期;
剪贴板历史因 passthrough 架构拿不到系统剪贴板而不做
(参见 [[passthrough-query-architecture]])。No hard dates; commit per milestone.
