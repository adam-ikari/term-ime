#!/usr/bin/env python3
"""Candidate paging must keep `candidate_window_` a legal index at all times.

Regression for the SIZE_MAX sentinel: `advance_candidate_window(-1)` used to
record "render the last page" as `candidate_window_ = numeric_limits<size_t>::max()`
and leave the convergence to `render_candidates_bar()`, which runs once per read
batch. Inside that batch the sentinel was live for every reader that selects:

* Enter did `select(static_cast<int>(SIZE_MAX))` = `select(-1)` -> rime returned
  nothing, the branch still called `ime_->cancel()`, and the whole composition
  disappeared (the key was reported Consumed, so the shell never saw it either).
* a page-up at the first rime page left the bar showing ONE candidate, because
  the sentinel clamped to `count-1` and the fit was computed over a 1-item tail.
* backward paging across a page boundary landed on the previous page's HEAD,
  skipping every window between it and the one the user had just left — the
  contract on brain/pages/candidate-bar-contract.md says paging must not skip.

Asserted on the real binary in a PTY, narrow terminal (46 cols -> ~3 candidates
per window), because none of it is observable without rime.
"""
import fcntl
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
COLS = 46
ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07]*\x07")

# rime's page size is what the "we crossed a page" bookkeeping counts against, so
# it is read from the shipped config rather than guessed.
with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "data", "rime-data", "default.yaml")) as fh:
    PAGE_SIZE = int(re.search(r"page_size:\s*(\d+)", fh.read()).group(1))


def strip_ansi(data: bytes) -> str:
    return OSC.sub("", ANSI.sub("", data.decode("utf-8", "replace")))


class Session:
    def __init__(self) -> None:
        self.home = tempfile.mkdtemp(prefix="paging-")
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
        """The candidate bar's numbered items, in display order."""
        frame = strip_ansi(self.read(0.8))
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

        # 1. ',' then Enter inside ONE read batch. Enter must commit the candidate
        #    the bar is showing, not cancel the composition. After a commit the bar
        #    is gone, so any CJK left in the frame is the shell echoing the text it
        #    was handed.
        shown = s.compose("nihao")
        head = shown[0]
        s.send(b",\r")
        time.sleep(1.0)
        frame = strip_ansi(s.read(1.0))
        ok = head in frame
        results.append(("page-up + Enter commits", ok))
        print("  [%s] page-up + Enter in one batch committed %r (bar was %s)"
              % ("PASS" if ok else "FAIL", head, shown))

        # 2. Page-up while already on the first window must leave the bar alone,
        #    not collapse it to a single candidate.
        before = s.compose("nihao")
        s.send(b",")
        time.sleep(0.5)
        after = s.bar()
        ok = len(before) > 1 and len(after) == len(before)
        results.append(("page-up keeps the bar full", ok))
        print("  [%s] page-up at the first window: %d -> %d items %s"
              % ("PASS" if ok else "FAIL", len(before), len(after), after))

        # 3. Backward paging across a rime page boundary must be the mirror of
        #    forward paging, not a jump to the previous page's head. Windows inside
        #    one page are disjoint from each other, so the boundary cannot be
        #    spotted from the candidate text; it is counted instead. Once a full
        #    page has been walked, the next '.' is the page change (the app pages by
        #    the width it showed, and window+step == page_size is exactly the
        #    condition it uses to hand the key to rime). One ',' back from the new
        #    page head must then meet the window that was on screen before it.
        seen = []
        forward = [s.compose("nihao")]
        seen += forward[0]
        for _ in range(12):
            s.send(b".")
            time.sleep(0.45)
            window = s.bar()
            if not window or window == forward[-1]:
                break  # ran out of candidates
            fresh = [c for c in window if c not in seen]
            if len(fresh) != len(window):
                break  # overlapping window: the paging model below no longer holds
            if len(seen) + len(fresh) > PAGE_SIZE:
                break  # this window belongs to the next page: we just crossed
            seen += fresh
            forward.append(window)
        crossed_the_page_boundary = len(seen) >= PAGE_SIZE
        s.send(b",")
        time.sleep(0.45)
        back = s.bar()
        came_from = forward[-1]
        ok = crossed_the_page_boundary and bool(set(back) & set(came_from)) and back != forward[0]
        results.append(("page-back does not skip windows", ok))
        print("  [%s] walked %d candidates over %d windows; last was %s, one ',' back shows %s"
              % ("PASS" if ok else "FAIL", len(seen), len(forward), came_from, back))

        s.send(b"\x1b")
        time.sleep(0.3)
    finally:
        s.close()

    passed = sum(1 for _, ok in results if ok)
    print("\n%d/%d checks passed" % (passed, len(results)))
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
