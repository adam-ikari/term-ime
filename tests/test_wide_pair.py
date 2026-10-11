#!/usr/bin/env python3
"""Overwriting or erasing half a wide glyph must survive a shell redraw.

Regression for the wide-pair invariant in src/terminal/screen.cpp (constraint 1
of brain/pages/review-fix-wide-char.md). `Cell::wide` is all `redraw_shell()`
looks at: it skips the column after a cell flagged wide. So a pair that is only
half written or half erased stays inconsistent in the shadow grid, and the
damage shows up on the next full repaint of the shell view -- not in the
original output, which the parser replays byte-for-byte.

To force that repaint the test opens the settings panel (a fullscreen ESC[2J
overlay) and closes it with ESC, which repaints the shell rows from the grid.
Two paths were missing before, both observable only there:

* a narrow glyph landing on the RIGHT half of a wide glyph left the left half
  flagged, so the repaint skipped the new glyph -- it was typed, stored, and
  never displayed again;
* erasing from the right half (EL) cleared that column alone, so the orphaned
  left half repainted as a whole glyph occupying a column that no longer exists.

The shell prints the wide glyph with octal escapes so the typed command line is
pure ASCII: the only CJK on screen is what the shell emitted, never an echo.
Each case starts from `clear`, because the previous case's row would otherwise
still be on the grid and could satisfy -- or spoil -- an absence assertion.
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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.environ.get("TERM_IME_BIN") or os.path.join(ROOT, "build", "term-ime")
COLS = 80
ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07]*\x07")

# 你 = U+4F60 = E4 BD A0, 好 = U+597D = E5 A5 BD. The ASCII markers are written
# as octal escapes too (Z = \132, W = \127), because the repainted frame contains
# the echoed command line as well: a literal Z in the typed text would make
# "the repaint drew Z" true even when the glyph column itself was skipped.
WIDE = r"\344\275\240\345\245\275"
MARK_Z = r"\132"


def strip_ansi(data: bytes) -> str:
    return OSC.sub("", ANSI.sub("", data.decode("utf-8", "replace")))


class Session:
    def __init__(self) -> None:
        self.home = tempfile.mkdtemp(prefix="wide-")
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

    def type(self, command: str, settle=0.7) -> None:
        self.send(command.encode() + b"\r")
        time.sleep(settle)
        self.read(0.8)

    def repainted_shell(self) -> str:
        """Open the settings overlay, close it, and return the repaint it triggers."""
        self.send(b"\x01s")
        if not self.wait_for("界面语言", seconds=10.0):
            raise AssertionError("settings overlay did not open (no 界面语言 on screen)")
        self.send(b"\x1b")
        time.sleep(0.8)
        return strip_ansi(self.read(1.5))

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
        s.read(0.5)

        # 1. Control: a wide glyph nothing touched must survive the repaint. Without
        #    this, the absence checks below could pass by losing the row entirely.
        s.type("clear")
        s.type(r"printf '" + WIDE + MARK_Z + r"\127\n'")
        frame = s.repainted_shell()
        ok = "好" in frame and "ZW" in frame
        results.append(("plain wide glyph survives repaint", ok))
        print("  [%s] control: after panel open/close, 好 present=%s, ZW present=%s"
              % ("PASS" if ok else "FAIL", "好" in frame, "ZW" in frame))

        # 2. 你好 then BS: the narrow glyph lands on 好's RIGHT half. The whole pair
        #    must be retired, so the repaint draws Z and no longer draws 好.
        s.type("clear")
        s.type(r"printf '" + WIDE + r"\b" + MARK_Z + r"\n'")
        frame = s.repainted_shell()
        ok_z = "Z" in frame
        ok_pair = "好" not in frame
        results.append(("narrow over right half survives repaint", ok_z))
        results.append(("narrow over right half retires the pair", ok_z and ok_pair))
        print("  [%s] Z over 好's right half: repaint draws Z=%s, still draws 好=%s (both halves must go)"
              % ("PASS" if ok_z else "FAIL", ok_z, not ok_pair))
        if not (ok_z and ok_pair):
            print("      repainted frame: %r" % frame.replace("\n", " | ")[:170])

        # 3. 你好 then BS then EL: the erase starts on 好's right half and must take
        #    the left half with it, or the orphan repaints as a whole glyph. 你 is at
        #    columns 0-1, outside the erased range, so it must still be drawn. That is
        #    what keeps this check from passing because the row simply vanished.
        s.type("clear")
        s.type(r"printf '" + WIDE + r"\b\033[K\n'")
        frame = s.repainted_shell()
        ok = "好" not in frame and "你" in frame
        results.append(("erase from right half clears the pair", ok))
        print("  [%s] EL from 好's right half: repaint still draws 好=%s, still draws 你=%s"
              % ("PASS" if ok else "FAIL", "好" in frame, "你" in frame))
        if not ok:
            print("      repainted frame: %r" % frame.replace("\n", " | ")[:170])

        s.send(b"\x1b")
        time.sleep(0.3)
    finally:
        s.close()

    passed = sum(1 for _, ok in results if ok)
    print("\n%d/%d checks passed" % (passed, len(results)))
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
