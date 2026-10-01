#!/usr/bin/env python3
"""模糊音 5 组独立开关的契约测试（跑真实二进制，PTY + 一次性 HOME）。

契约（见 brain/pages/fuzzy-pinyin-toggle.md）：
- 每组可独立开关：zh_z 平翘舌 / n_l / r 系 / hu_f / nose 前后鼻音
- 组开 → 对应的「模糊音专属字」出现在候选里；组关 → 消失
- 全关 = 精确拼音
- 部分开启时引擎合成 per-combination schema（luna_pinyin_simp_fuzzy_<sig>）
"""
import fcntl
import json
import os
import pty
import re
import select
import shutil
import struct
import sys
import tempfile
import termios
import time

BIN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "term-ime")

ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07]*\x07")

# (音节, 该字只在*该组*模糊音开启时出现, 所属组) —— 每组一个专属探针字
GROUP_PROBES = [
    ("la", "那", "n_l"),      # n/l 互换
    ("zi", "之", "zh_z"),     # z→zh
    ("fan", "方", "nose"),    # an/ang
    ("fen", "风", "nose"),    # en/eng
]

RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append((name, bool(ok)))
    print("  [%s] %-48s %s" % ("PASS" if ok else "FAIL", name, detail[:70]), flush=True)


def wait_file(path, seconds=240.0):
    """Bounded wait for librime's background deploy to leave this file behind.

    A single os.path.exists() sample reads "not deployed yet" as "never
    deploys": on a fresh CI runner the prism for a generated schema shows up
    seconds after the status bar is already usable. The CI runner is
    ~3x slower at compiling per-combination prisms than the local box, so 90s
    was flaky-red on every push. 240s lets the job finish and report which
    combos missed the window instead of tripping the job timeout; the
    per-combination deploy itself is tracked separately.
    """
    end = time.time() + seconds
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.25)
    return False


def wait_generated_prism(udir, seconds=240.0):
    """Wait for the prism of any generated per-combination fuzzy schema."""
    build = os.path.join(udir, "build")
    end = time.time() + seconds
    while time.time() < end:
        try:
            names = os.listdir(build)
        except OSError:
            names = []
        if any(n.startswith("luna_pinyin_simp_fuzzy_") and n.endswith(".prism.bin") for n in names):
            return True
        time.sleep(0.25)
    return False


def strip_ansi(data: bytes) -> str:
    return OSC.sub("", ANSI.sub("", data.decode("utf-8", "replace")))


class Session:
    """以指定 fuzzy_groups 配置启动一个一次性 app 实例。"""

    def __init__(self, groups) -> None:
        self.home = tempfile.mkdtemp(prefix="fg-")
        env = dict(os.environ)
        env.update(
            HOME=self.home,
            XDG_CONFIG_HOME=os.path.join(self.home, ".config"),
            XDG_DATA_HOME=os.path.join(self.home, ".local", "share"),
            SHELL="/bin/sh",
            TERM="xterm-256color",
        )
        cfg_dir = os.path.join(self.home, ".config", "term-ime")
        os.makedirs(cfg_dir, exist_ok=True)
        with open(os.path.join(cfg_dir, "config.json"), "w") as f:
            # log_level=info so the deploy lines ("deploying generated schema
            # ...") reach the log dump below; the default "warn" would leave the
            # interesting part out exactly when it is needed.
            json.dump({"fuzzy_groups": groups, "log_level": "info"}, f)
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.execvpe(BIN, [BIN], env)
            except Exception:
                os._exit(127)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 140, 0, 0))
        flags = fcntl.fcntl(self.fd, fcntl.F_GETFL)
        fcntl.fcntl(self.fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def read(self, seconds=1.0, quiet=0.10) -> bytes:
        out = b""
        end = time.time() + seconds
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], quiet)
            if not ready:
                break
            try:
                chunk = os.read(self.fd, 65536)
            except BlockingIOError:
                continue
            except OSError:
                break
            if not chunk:
                break
            out += chunk
        return out

    def wait_for(self, pattern: str, seconds=60.0) -> bool:
        rx = re.compile(pattern)
        buf = b""
        end = time.time() + seconds
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], 0.1)
            if not ready:
                continue
            try:
                chunk = os.read(self.fd, 65536)
            except BlockingIOError:
                continue
            except OSError:
                break
            if not chunk:
                break
            buf += chunk
            if rx.search(strip_ansi(buf)):
                return True
        return False

    def send(self, data: bytes) -> None:
        try:
            os.write(self.fd, data)
        except OSError:
            pass

    def bar(self):
        frame = strip_ansi(self.read(0.9))
        lines = [l for l in frame.splitlines() if re.search(r"\[拼\]", l)]
        if not lines:
            return []
        return [m[1] for m in re.findall(r"(\d)\.(\S+)", lines[-1])]

    def candidates(self, pinyin, pages=3) -> str:
        """Type `pinyin`, page a few times, return all candidates as one string."""
        self.read(0.3)
        self.send(pinyin.encode())
        time.sleep(0.7)
        collected = []
        for _ in range(pages):
            collected += self.bar()
            self.send(b".")
            time.sleep(0.4)
        self.send(b"\x1b")
        time.sleep(0.35)
        return "".join(collected)

    def close(self) -> None:
        try:
            os.kill(self.pid, 9)
            os.waitpid(self.pid, 0)
        except Exception:
            pass
        shutil.rmtree(self.home, ignore_errors=True)


def dump_rime_state(home: str, udir: str, label: str, lines: int = 60) -> None:
    """Print what the app left behind for a session whose prism never showed up.

    The log file lives under HOME, which close() deletes, and the two questions
    a "prism missing" failure has are (a) did the app say why, and (b) what is
    actually on disk in the rime user dir. Both are gone by the time the next
    case starts, so capture them here while the directory still exists.
    """
    print(f"----- {label}: rime user dir {udir} -----", flush=True)
    for sub in ("", "build"):
        d = os.path.join(udir, sub) if sub else udir
        try:
            entries = sorted(os.listdir(d))
        except OSError as e:
            print(f"  [{sub or '.'}] cannot list: {e}")
            continue
        for name in entries:
            full = os.path.join(d, name)
            try:
                size = os.path.getsize(full)
            except OSError:
                size = -1
            print(f"  [{sub or '.'}] {name} ({size} bytes)")
    log_path = os.path.join(home, ".cache", "term-ime", "term-ime.log")
    try:
        with open(log_path, encoding="utf-8", errors="replace") as f:
            tail = f.readlines()[-lines:]
    except OSError as e:
        print(f"----- {label}: cannot read {log_path}: {e} -----")
        return
    print(f"----- {label}: rime log tail ({log_path}) -----")
    for line in tail:
        print("  " + line.rstrip())
    print(f"----- end {label} -----")


def run_commit_case():
    """Enter must commit the composition to the SHELL, and only commit.

    Every other check here reads the candidate bar, so the whole commit path
    (take_commit() → send_to_shell()) had no coverage at all — which is how
    Enter could forward a bare \\r to the child while the pinyin buffer stayed
    up, and a digit-select could commit a second time on the next Enter.

    How to observe it without looking at the bar: press Enter twice.
      - correct: the first Enter commits 「你好」 onto the shell's input line
        (no newline — same as fcitx5/ibus + rime), so the second Enter *runs*
        「你好」 and the shell reports `not found`.
      - broken: the first Enter is forwarded as a bare \\r, committing nothing;
        the pinyin is still in the IME, so the second Enter runs an empty line
        and nothing is reported.

    So `not found` after the second Enter is the positive signal that the commit
    reached the shell. Asserting on a fresh frame instead (does the bar still
    show candidates?) does NOT work: the broken build never repaints after
    Enter, so the old bar is simply still on screen and a fresh read is empty —
    identical to the fixed build. That distinction is why this probe exists.
    """
    s = Session([])  # 精确拼音，避免 per-combination prism 干扰这条断言
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]", seconds=120.0):
            check("commit: ready", False)
            return
        s.send(b"\x01 ")                      # Ctrl+Space → 中文
        s.wait_for(r"\[拼\]", seconds=8.0)
        s.send(b"nihao")
        time.sleep(1.2)
        s.send(b"\r")                         # Enter 提交（不执行）
        time.sleep(1.0)
        s.send(b"\r")                         # 再回车：把行首内容暴露出来
        time.sleep(1.2)
        out = strip_ansi(s.read(1.5))
        check("commit: Enter committed pinyin to the shell",
              "not found" in out, out[-60:].replace("\n", " "))
    finally:
        s.close()


def run_unswallowed_digit_case():
    """A digit the IME did not consume must still reach the shell.

    `[` is a punctuation key, and rime answers it by committing 「 and *opening a
    bracket-picker composition* whose menu holds 「 【 〔 ［. The digit branch
    then treats the next key as a slot selection, finds slot 7 past the 4 shown,
    logs "Candidate slot 7 beyond the 0 shown" and drops the byte. So typing
    `[7` produced 「 with the 7 silently gone — the user cannot see that a
    keystroke was eaten and cannot recover it.

    The rule this asserts is the same one the punctuation branch already
    follows: a byte the IME did not use belongs to the PTY.

    How to see it: send `[7`, ESC to drop the composition, switch back to
    English so later keys bypass the IME, then Enter to run whatever is on the
    shell's line. With the digit forwarded the shell reports `7: not found`;
    with it dropped the line is empty and nothing is reported at all.
    """
    s = Session([])
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]", seconds=120.0):
            check("swallow: ready", False)
            return
        s.send(b"\x01 ")                      # Ctrl+Space → 中文
        s.wait_for(r"\[拼\]", seconds=8.0)
        s.send(b"[7")
        time.sleep(1.6)
        s.send(b"\x1b")                       # ESC：取消括号候选组合
        time.sleep(1.2)
        s.send(b"\x01 ")                      # 切回英文，后续按键直达 shell
        time.sleep(0.6)
        s.send(b"\r")                         # 执行行首，暴露内容
        time.sleep(1.4)
        out = strip_ansi(s.read(1.5))
        check("swallow: unclaimed digit reached the shell",
              "7: not found" in out, out[-60:].replace("\n", " "))
    finally:
        s.close()


def run_multi_commit_case():
    """Each Enter must commit ITS OWN segment and leave nothing behind.

    run_commit_case only ever commits one segment, so it cannot see whether the
    composition is actually torn down between commits. Two failure modes need
    their own coverage:

      - Enter does not end the composition: the next segment's pinyin lands in
        the *old* buffer, and one commit later swallows everything typed so far.
      - Enter commits twice: the shell's line carries a stray duplicate.

    Probe: for each segment, send the pinyin, Enter (commit), Enter again (run
    the line, exposing what the first Enter put there). Then `not found` must
    appear once per segment, and each occurrence must name exactly one segment.

    The second Enter is not optional bookkeeping. Without it the final line only
    shows the LAST commit, and the broken build — which never commits at all —
    looks identical: nothing to distinguish. Verified against the pre-fix binary
    (`git show '30c0e2f^:src/core/app.cpp'`): the fixed build reports three
    separate `not found` lines, the broken build reports zero.
    """
    segments = [(b"nihao", "你好"), (b"shijie", "世界"), (b"zhongguo", "中国")]
    s = Session([])
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]", seconds=120.0):
            check("multi-commit: ready", False)
            return
        s.send(b"\x01 ")
        s.wait_for(r"\[拼\]", seconds=8.0)
        collected = []
        for pinyin, _ in segments:
            s.send(pinyin)
            time.sleep(1.4)
            s.send(b"\r")                        # 提交本段
            time.sleep(1.2)
            s.send(b"\r")                        # 执行行首，暴露本段内容
            time.sleep(1.4)
            collected.append(strip_ansi(s.read(0.6)))
        hits = [ln.strip() for seg in collected
                for ln in seg.splitlines() if "not found" in ln]
        check("multi-commit: one commit per segment, not one for all",
              len(hits) == len(segments),
              "got %d: %s" % (len(hits), hits[-3:]))
        for idx, (pinyin, want) in enumerate(segments):
            # Index rather than zip: with fewer `hits` than segments a zip loop
            # body would simply not run and every assertion below would pass
            # vacuously — the count check would catch it, but this one would not.
            hit = hits[idx] if idx < len(hits) else ""
            others = [w for _, w in segments if w != want]
            check("multi-commit: %s committed on its own" % pinyin.decode(),
                  bool(hit) and want in hit and not any(o in hit for o in others),
                  hit[-40:] or "(no commit seen)")
    finally:
        s.close()


def run_group_case(groups, label):
    """以指定组集合启动，逐组探针：开→候选含专属字，关→不含。"""
    s = Session(groups)
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]", seconds=120.0):
            check(f"{label}: ready", False)
            return
        s.send(b"\x01 ")
        s.wait_for(r"\[拼\]", seconds=8.0)
        # 部分开启走的是合成出来的 per-combination schema，它要等 librime 后台部署
        # 完才有 prism；部署没完就打字，拿到的是"这组模糊音不生效"的候选 —— 那是
        # 就绪窗口，不是缺陷。
        if 0 < len(groups) < 5:
            udir = os.path.join(s.home, ".local", "share", "term-ime")
            if not wait_generated_prism(udir):
                dump_rime_state(s.home, udir, label)
                check(f"{label}: generated prism deployed", False, udir)
                return
            time.sleep(0.5)
        for pinyin, char, group in GROUP_PROBES:
            got = s.candidates(pinyin)
            expect = group in groups
            ok = (char in got) == expect
            word = "has" if expect else "lacks"
            check(f"{label}: {group} {'ON ' if expect else 'OFF'} {pinyin} {word} {char}", ok, got[:40])
    finally:
        s.close()


def main() -> int:
    # 全开：每组探针字都该出现
    run_group_case(["zh_z", "n_l", "r", "hu_f", "nose"], "all-on")
    # 全关：精确拼音
    run_group_case([], "precise")
    # 只开 n_l：n_l 探针在，其他组探针不在
    run_group_case(["n_l"], "n_l-only")
    # 只开 zh_z：反向验证
    run_group_case(["zh_z"], "zh_z-only")

    run_commit_case()
    run_multi_commit_case()
    run_unswallowed_digit_case()

    # 动态组合 schema 物化：部分开启时用户目录应有生成的 schema + prism
    s = Session(["n_l", "nose"])
    try:
        ok = s.wait_for(r"\[EN\]|\[拼\]")
        check("subset: ready", ok)
        udir = os.path.join(s.home, ".local", "share", "term-ime")
        gen_schema = os.path.join(udir, "luna_pinyin_simp_fuzzy_n_l_nose.schema.yaml")
        gen_prism = os.path.join(udir, "build", "luna_pinyin_simp_fuzzy_n_l_nose.prism.bin")
        check("subset: generated schema written", wait_file(gen_schema), gen_schema)
        if not wait_file(gen_prism):
            dump_rime_state(s.home, udir, "subset")
            check("subset: prism deployed", False, gen_prism)
        else:
            check("subset: prism deployed", True, gen_prism)
    finally:
        s.close()

    passed = sum(1 for _, ok in RESULTS if ok)
    print("\n===== %d/%d PASS =====" % (passed, len(RESULTS)))
    for name, ok in RESULTS:
        if not ok:
            print("  FAILED:", name)
    return 0 if passed == len(RESULTS) else 1


if __name__ == "__main__":
    sys.exit(main())
