# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

term-ime is a virtual terminal (PTY) that runs in the Linux TTY and ships a built-in multilingual input method (IME) backed by librime, with optional LLM-based candidate ranking via llama.cpp. Linux-only, C++17, CMake build. UI is rendered with FTXUI.

> Note: `README.md` is partially out of date — it mentions an ONNX neural ranker and `Shift+Space` for mode toggle; the actual code uses llama.cpp and `Ctrl+A Space`. `docs/design.md` (early design sketch, hand-rolled pinyin dict) has been removed. When updating docs, edit `README.md` and trust the source over it.

## Build & Run

```bash
make build            # full clean CMake Release build (deletes build/)
# or incremental:
cmake --build build -j$(nproc)

./build/term-ime      # must run in a real TTY or a terminal supporting alt screen
```

`make build` does `rm -rf build` first, so for quick "edit one line, test" iterations use the incremental path instead:

```bash
cmake --build build -j$(nproc) && ./build/test-input-e2e   # build + run one test, no full clean
```

A config path can be passed as the first CLI arg to override the default location: `./build/term-ime /path/to/config.json`.

Submodules are required and built from source (FTXUI, spdlog, nlohmann_json, googletest, librime, llama.cpp, sml, utf8proc) — clone with `--recursive` or run `git submodule update --init --recursive`. True system dependencies are `libuv1-dev` and curl (`find_library ... uv` and `find_package(CURL)`); `librime-dev` is listed in README/CI but the build itself uses `deps/librime` (the README lists `rime-data-luna-pinyin` for the pinyin schema data).

CMake acceleration options (off by default): `-DLLAMA_USE_CUDA=ON`, `-DLLAMA_USE_METAL=ON`, `-DLLAMA_USE_VULKAN=ON`, `-DLLAMA_USE_NPU=ON`.

## Tests

```bash
make test                         # runs ./build/term-ime-tests (the gtest binary)
cd build && ctest --output-on-failure   # CTest; only term-ime-tests is gtest_discover_tests'd
```

There are **four** test executables, but CMake only registers `term-ime-tests` with CTest via `gtest_discover_tests`:

- `term-ime-tests` — UTF-8, config, IME state (gtest; entry in `tests/test_main.cpp`)
- `test-ui-jsx` — UI JSX components (plain `main()`, custom checks)
- `test-settings` — settings panel (plain `main()`, custom checks)
- `test-input-e2e` — input processor state machine, plain `assert`-based (not gtest)

So `--gtest_filter` only applies to `term-ime-tests`. Run an individual non-ctest binary directly, e.g. `./build/test-input-e2e`. Run a single gtest case:

```bash
./build/term-ime-tests --gtest_filter='TestName.*'
```

`tests/*.py` (e2e over a real PTY) and `test/test_all.sh` are standalone scripts — **not** wired into `make`/`ctest`. Invoke them manually with `python3 tests/test_e2e.py` etc.; they expect `./build/term-ime` to exist.

## Formatting

```bash
make format    # clang-format -i over src/ and tests/
```

`.clang-format`: Google base, 4-space indent, 120-col limit, attached braces, `SortIncludes: false`. CI runs a `--dry-run --Werror` format check on `src/` (continue-on-error, so it won't fail the build). Format before committing.

There is **no** cppcheck / clang-tidy / sanitizer configured — static checking is limited to the compiler's `-Wall -Wextra` (`CMakeLists.txt` top). Don't ignore build warnings; fix them at the source.

## Code style (docs/CODE_STYLE.md — followed throughout)

- Types/structs/enums + enum values: `PascalCase`. Locals & free functions: `snake_case`. Members: `snake_case_` (trailing underscore). Constants: `kPascalCase`. Getters have no `get_` prefix; setters are `set_*`.
- `#pragma once` for headers. Include order: own header → project headers → third-party → system → C stdlib. `SortIncludes: false`, so preserve this order manually.
- Smart pointers / RAII only; no `using namespace std;`; no C-style casts; no exceptions for control flow (log via `spdlog` and return status). Logging goes through `spdlog` (runtime file log at `~/.cache/term-ime/term-ime.log`).

## Architecture

Four CMake static libraries compose the app; `main.cpp` is thin wiring around them:

- **`term-core`** — `EventLoop` (libuv: fd polling, signals, timers, `queue_work` thread-pool, `schedule` for next-tick), `App` (top-level orchestrator owning all subsystems and routing PTY/keyboard/resize events), `InputProcessor` (a Boost.SML state machine that classifies each input byte: forward to shell, detect Ctrl+A+Space mode toggle, or reassemble escape sequences), `Config`.
- **`term-terminal`** — `Pty` (forks the shell on a pseudo-terminal), `Screen` (cell grid buffer, cursor, scroll), `Parser` (VT100/CSI escape-sequence → screen), plus the `ui/` rendering layer.
- **`term-ime-lib`** — `ImeEngine` abstract interface with `RimeIme` (librime wrapper: schema selection, compose/select/page), `LanguageManager` (config-driven language switching, not hardcoded), `CandidateRanker` interface with `NullRanker` and `LlamaRanker` (async, lazy-loaded llama.cpp ranker running on a worker thread with a task queue), `KaomojiLib`, `ModelDownloader` (curl), and `util/` (`utf8`, `i18n`, `logger`).
- **`term-ime`** (executable) — `main.cpp` sets up file logging, loads `AppConfig`, inits `I18n` from `active_language`, creates `EventLoop` + `App`, and registers fd/signal callbacks. Notable behavior: when the settings panel is visible, PTY data is ignored so the shell keeps running but its output isn't shown; PTY EOF triggers graceful exit.

### UI layer (`src/ui/`)

A small JSX-style declarative framework over FTXUI: `jsx.hpp` re-exports FTXUI `Element`/`Decorator` and wrappers (`Text`, `HBox`, `VBox`, color/size decorators). `components.hpp` builds reusable components (`CandidateBar`, `ModeIndicator`, `StatusBar`, `AIIndicator`) as pure functions from props structs → `Element`. `settings.hpp` is the in-app settings panel (`SettingsState`, `settings_handle_key`, `settings_init`/`settings_apply` against `AppConfig`). `renderer.cpp` owns the raw TTY (termios raw mode, alt screen) and composes the screen + candidate bar + status bar + settings panel each frame.

### Key flows

- **Input → IME vs shell** (`app.cpp` `on_keyboard_data`): `main.cpp` feeds stdin bytes to `App::on_keyboard_data`, which runs each byte through `InputProcessor` (a Boost.SML state machine). `Ctrl+A` (byte `0x01`) is a *prefix* state — the next byte decides:
  - `Ctrl+A Space` → toggle Chinese/English mode (consumed, not forwarded)
  - `Ctrl+A A` → toggle AI ranking (`App::toggle_ai_ranking`)
  - `Ctrl+A S` → toggle settings panel
  - `Ctrl+A Ctrl+A` → forward a literal `0x01` to the shell
  - other key → forward `Ctrl+A` + that key to the shell
  - (README says `Ctrl+A Space`, `test/TEST_CASES.md` says `Shift+Space` — the code is `Ctrl+A Space`.)
  - In Chinese mode (non-composing), lowercase `a-z` start IME composition; selected candidates are written to the PTY as UTF-8. While composing/selecting: `1-9` pick a candidate, `Space` picks the first, `Backspace` (`0x08`/`0x7f`) cancels, further `a-z` extend the buffer, and everything else (arrow keys / escape sequences) is ignored. All other bytes forward to the shell. The settings panel, when visible, captures keys and PTY output is suppressed.
- **Async ranking**: `LlamaRanker::rank_async` enqueues a `RankTask`; the worker thread lazily loads the GGUF model, builds a prompt, parses the LLM's ranking, and invokes the callback. `App` caches `last_candidates_` and re-renders on completion. Model download is likewise async via `ModelDownloader` (curl) with progress reported to the status bar.
- **Config & i18n**: `AppConfig` (JSON; `AppConfig::default_path()` uses `$XDG_CONFIG_HOME/term-ime/config.json`, falling back to `~/.config/term-ime/config.json`) drives languages, rime dirs, llama ranker, UI language, log level. Override the path with `./build/term-ime <path>`. `I18n` loads translations from `data/translations/*.json` keyed by `ui_language`. `main.cpp` maps `active_language` → `I18n::Lang` and overrides `config.shell` from `$SHELL` when unset *or* when it's the default `/bin/bash`.
- **Logging**: `main.cpp` writes to `~/.cache/term-ime/term-ime.log` (falls back to `/tmp` when `$HOME` is unset), level from `config.log_level`. `test/test_all.sh` greps this log to verify startup, so a missing log file is the first sign of a failed launch.

## Conventions to preserve

- Languages are config-driven — do not hardcode language/mode checks; go through `LanguageManager` and `LanguageConfig`.
- The IME layer is abstracted behind `ImeEngine` / `CandidateRanker` to allow swapping backends (librime vs. others; null vs. llama ranking). Keep new ranking logic behind `CandidateRanker`.
- Async/off-thread work goes through `EventLoop::queue_work` or dedicated worker threads (see `LlamaRanker`); never block the input path.
- Add new tests as a CMake `add_executable` in `CMakeLists.txt` and link the relevant `term-*` lib. Mirror the existing `tests/` naming (`test_<area>.cpp`).
