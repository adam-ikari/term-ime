#!/usr/bin/env python3
"""One mistyped config value must not throw the whole config file away.

Regression for the sanitization gap in src/core/config.cpp (brain/pages/
config-load-sanitization.md). The loader used to read every key through
nlohmann's `value(key, default)`, which THROWS on a type mismatch rather than
returning the default: a single hand-edit with the wrong JSON type -- "on"
instead of true, a bare string where an array belongs -- discarded the entire
file and every correct key in it. The user then sees defaults, with nothing on
screen explaining why their settings stopped applying.

Asserted through the real app because that is where the damage is visible: the
candidate cap from the same file still has to take effect, and the rejected key
names have to appear in the log the user is told to open.
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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.environ.get("TERM_IME_BIN") or os.path.join(ROOT, "build", "term-ime")
COLS = 110
ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07]*\x07")

# The cap is the observable good key; the two bad ones are wrong on purpose.
CAP = 3
CONFIG = {
    "max_candidates": CAP,
    "show_mode_indicator": "on",  # must be a bool
    "fuzzy_groups": "zh_z",       # must be an array
}


def strip_ansi(data: bytes) -> str:
    return OSC.sub("", ANSI.sub("", data.decode("utf-8", "replace")))


class Session:
    def __init__(self) -> None:
        self.home = tempfile.mkdtemp(prefix="cfgtypes-")
        config_dir = os.path.join(self.home, ".config", "term-ime")
        os.makedirs(config_dir, exist_ok=True)
        with open(os.path.join(config_dir, "config.json"), "w") as fh:
            json.dump(CONFIG, fh, indent=2)
        self.log = os.path.join(self.home, ".cache", "term-ime", "term-ime.log")
        env = dict(os.environ)
        env.update(
            HOME=self.home,
            XDG_CONFIG_HOME=os.path.join(self.home, ".config"),
            XDG_DATA_HOME=os.path.join(self.home, ".local", "share"),
            SHELL="/bin/sh",
            TERM="xterm-256color",
        )
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            try:
                os.execvpe(BIN, [BIN], env)
            except Exception:
                os._exit(127)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, COLS, 0, 0))
        flags = fcntl.fcntl(self.fd, fcntl.F_GETFL)
        fcntl.fcntl(self.fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def read(self, seconds=1.0, quiet=0.15) -> bytes:
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
        """The candidate bar's numbered items, in display order."""
        frame = strip_ansi(self.read(0.9))
        lines = [l for l in frame.splitlines() if re.search(r"\[拼\]", l)]
        if not lines:
            return []
        return [m[1] for m in re.findall(r"(\d)\.(\S+)", lines[-1])]

    def compose(self, pinyin):
        self.send(b"\x1b")
        time.sleep(0.3)
        self.read(0.4)
        self.send(pinyin.encode())
        time.sleep(1.0)
        return self.bar()

    def close(self) -> None:
        try:
            os.kill(self.pid, 9)
            os.waitpid(self.pid, 0)
        except Exception:
            pass
        shutil.rmtree(self.home, ignore_errors=True)


def main() -> int:
    s = Session()
    results = []
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]"):
            print("  [FAIL] app did not become ready")
            return 1
        s.send(b"\x01 ")  # Chinese mode
        s.wait_for(r"\[拼\]", seconds=8.0)
        s.read(0.5)

        # 1. The cap in the same file as the two bad keys must still apply. With a
        #    110-column terminal the width fits more than CAP candidates, so a
        #    shorter bar can only come from max_candidates being honoured.
        shown = s.compose("nihao")
        ok = len(shown) == CAP
        results.append(("good key in a partly-mistyped file still applies", ok))
        print("  [%s] max_candidates=%d -> bar shows %d items: %s"
              % ("PASS" if ok else "FAIL", CAP, len(shown), shown))

        # 2. The rejected names must be reported, in the log the user is sent to.
        s.send(b"\x1b")
        time.sleep(0.6)
        note = ""
        for _ in range(20):
            try:
                with open(s.log) as fh:
                    text = fh.read()
            except OSError:
                time.sleep(0.2)
                continue
            m = [l for l in text.splitlines() if "ignored config key" in l]
            if m:
                note = m[-1]
                break
            time.sleep(0.2)
        reported = all(k in note for k in ("show_mode_indicator", "fuzzy_groups"))
        results.append(("ignored keys are named in the log", reported))
        print("  [%s] log note: %r" % ("PASS" if reported else "FAIL", note or "<none>"))
    finally:
        s.close()

    passed = sum(1 for _, ok in results if ok)
    print("\n%d/%d checks passed" % (passed, len(results)))
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
