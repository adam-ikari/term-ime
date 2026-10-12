#!/usr/bin/env python3
"""The candidate bar must say whether it can still be paged.

'‹' sits before the first candidate and means a ',' reaches something the user
has not seen (an earlier window of this rime page, or the previous rime page);
'›' sits after the last one and means a '.' does. Both flank the candidate run —
a right-aligned arrow alone behind a hundred filler columns does not read as
part of the list — and both are drawn out of the candidates' width budget, so
the line they sit on must still fit the terminal (candidate-bar-contract:
显示集合 = 可选集合, 且候选栏绝不越界).

Why rime's own paging is needed for the arrows: librime hands back ONE page of
candidates (menu/page_size), so an empty tail reads either as "that was all" or
"the rest is on the next page" — only the engine knows which. Probing by paging
would move the page under the user, so it is reported instead (ImeMenuPaging).

Driven on the real binary in a PTY at a narrow width (46 cols: 3 candidates in the
first window of a page, 2 once both arrows are reserved) and at a wide one (120
cols, a whole rime page in one window), because the window-level arrows are not
observable at any other width.
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
import unicodedata

BIN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "term-ime")
COLS = 46
ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07]*\x07")

RESULTS = []

# rime's page size is what the "we crossed a page" bookkeeping counts against, so
# it is read from the shipped config rather than guessed.
with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "data", "rime-data", "default.yaml")) as fh:
    PAGE_SIZE = int(re.search(r"page_size:\s*(\d+)", fh.read()).group(1))


def check(name, ok, detail=""):
    RESULTS.append((name, bool(ok)))
    print("  [%s] %-42s %s" % ("PASS" if ok else "FAIL", name, detail[:80]), flush=True)


def strip_ansi(data: bytes) -> str:
    return OSC.sub("", ANSI.sub("", data.decode("utf-8", "replace")))


def dwidth(s: str) -> int:
    """Display columns, matching the bar's own width accounting: CJK is 2,
    everything else (including ‹ ›, East-Asian-ambiguous) is 1."""
    return sum(2 if unicodedata.east_asian_width(c) in ("W", "F") else 1 for c in s)


def compact(s: str) -> str:
    """Collapse the filler between the candidates and the right-aligned arrows,
    so a failure line shows both ends instead of 40 columns of spaces."""
    return re.sub(r" {2,}", " ", s).strip()


class Session:
    def __init__(self) -> None:
        self.home = tempfile.mkdtemp(prefix="hint-")
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
            if rx.search(strip_ansi(chunk)):
                return True
        return False

    def send(self, data: bytes) -> None:
        try:
            os.write(self.fd, data)
        except OSError:
            pass

    def resize(self, cols: int) -> None:
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, cols, 0, 0))
        time.sleep(0.6)
        self.read(0.5)

    def bar_line(self) -> str:
        """The candidate bar's row as text (arrows included).

        Every bar write is "ESC[2K<row>", and the row is rewritten in place with
        no newline — so several frames land on ONE line when read raw, and the
        merged text shows candidates from different windows at once. Splitting
        on the erase-line sequence isolates a frame per chunk.
        """
        text = self.read(0.8).decode("utf-8", "replace")
        frames = [OSC.sub("", ANSI.sub("", c)) for c in text.split("\x1b[2K")]
        lines = [l for f in frames for l in f.splitlines() if re.search(r"\[拼\]", l)]
        return lines[-1] if lines else ""

    def items(self, line: str):
        """The candidate texts the bar is showing, in display order."""
        return [m[1] for m in re.findall(r"(\d)\.(\S+)", line)]

    def compose(self, pinyin) -> str:
        self.send(b"\x1b")
        time.sleep(0.3)
        self.read(0.4)
        self.send(pinyin.encode())
        time.sleep(1.0)
        return self.bar_line()

    def close(self) -> None:
        try:
            os.kill(self.pid, 9)
            os.waitpid(self.pid, 0)
        except Exception:
            pass
        shutil.rmtree(self.home, ignore_errors=True)


def main() -> int:
    s = Session()
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]"):
            print("  [FAIL] app did not become ready")
            return 1
        s.send(b"\x01 ")  # Chinese mode
        s.wait_for(r"\[拼\]", seconds=8.0)
        s.read(0.5)

        # 1. First window of a page with more behind it: '›' only.
        first = s.compose("nihao")
        check("first window shows ›", first.rstrip().endswith("›"), repr(first[-24:]))
        check("first window hides ‹", "‹" not in first, repr(first[-24:]))
        check("bar still fits the terminal", 0 < dwidth(first) <= COLS, "%d cols" % dwidth(first))
        check("window is a real slice", len(s.items(first)) >= 2, repr(s.items(first)))

        # 2. Walk forward across the rime page boundary. Consecutive windows of
        #    ONE rime page are disjoint and so is the head of the next page, so
        #    the boundary cannot be spotted from the candidate texts — it is
        #    counted against rime's page size instead (same model as
        #    test_candidate_paging.py). Everything walked is still reachable
        #    backwards, so '‹' must be there from the second window on.
        seen = list(s.items(first))
        prev_window = first
        page_head = None
        for _ in range(8):
            s.send(b".")
            time.sleep(0.5)
            line = s.bar_line()
            check("bar keeps fitting the terminal", 0 < dwidth(line) <= COLS, "%d cols" % dwidth(line))
            items = s.items(line)
            if not items:
                continue
            if len(seen) + len(items) > PAGE_SIZE:
                page_head = line  # this window is on the next rime page
                break
            if items != s.items(first):
                check("later window shows ‹", "‹" in line, repr(line[-24:]))
            seen += items
            prev_window = line
        check("forward paging reached a new rime page", page_head is not None, repr(s.items(page_head or "")))
        if page_head:
            check("new page shows ‹", "‹" in page_head, repr(page_head[-24:]))
            check("new page still fits", dwidth(page_head) <= COLS, "%d cols" % dwidth(page_head))

            # 3. One ',' back must land on the last window of the page it left —
            #    the one the user was standing on — not on that page's head.
            s.send(b",")
            time.sleep(0.5)
            back = s.bar_line()
            back_items = s.items(back)
            check("page-back shows the window it left", back_items == s.items(prev_window),
                  "%s vs %s" % (back_items, s.items(prev_window)))
            check("page-back shows ‹", "‹" in back, repr(compact(back)))

            # The arrows FLANK the candidate run — '‹' before the first item and
            # '›' after the last one — instead of sitting at the ends of the line.
            if back_items and "1." in back and "2." in back:
                check("‹ sits before the first candidate", back.find("‹") < back.find("1."), repr(compact(back)))
                check("› sits after the last candidate", back.rfind("›") > back.rfind("2."), repr(compact(back)))

            # 4. The arrows did not steal a digit slot: '1' still commits the
            #    first candidate the bar is showing.
            if back_items:
                head = back_items[0]
                s.send(b"1\r")
                time.sleep(1.0)
                frame = strip_ansi(s.read(1.0))
                check("digit selects what the bar shows", head in frame, "%r in frame=%s" % (head, head in frame))

        # 5. The arrows' columns are part of the width budget, so a resize has
        #    to recompute both the fit and the hint: the bar must still fit the
        #    new width, and an arrow paging can act on must still be there. A
        #    wide terminal shows a whole rime page in one window, so its first
        #    window only has the cross-page half of the hint ('›'); one '.' moves
        #    a page and the other half appears from the engine (page_no > 0).
        for wide in (120, COLS):
            s.resize(wide)
            first_window = s.compose("nihao")  # the bar is gone after a commit; start over
            check("%d列 bar fits" % wide, 0 < dwidth(first_window) <= wide, "%d cols" % dwidth(first_window))
            check("%d列 first window shows ›" % wide, first_window.rstrip().endswith("›"), repr(compact(first_window)))
            s.send(b".")
            time.sleep(0.6)
            moved = s.bar_line()
            check("%d列 next window fits" % wide, 0 < dwidth(moved) <= wide, "%d cols" % dwidth(moved))
            check("%d列 next window shows ‹" % wide, "‹" in moved, repr(compact(moved)))

        s.send(b"\x1b")
        time.sleep(0.3)
    finally:
        s.close()

    passed = sum(1 for _, ok in RESULTS if ok)
    print("\n%d/%d checks passed" % (passed, len(RESULTS)))
    return 0 if passed == len(RESULTS) else 1


if __name__ == "__main__":
    sys.exit(main())
