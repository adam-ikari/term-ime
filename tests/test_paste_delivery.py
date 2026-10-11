#!/usr/bin/env python3
"""End-to-end paste delivery check for term-ime.

Answers one question with bytes rather than opinion: when a terminal hands
term-ime a big paste, how much of it reaches the program reading the inner
pty, and does what arrives keep the original order?

Run:  python3 tests/test_paste_delivery.py [--bytes N] [--stall SEC]

--stall makes the reader fall asleep first, which is the case that overflows
the outbound queue. Without it the reader drains continuously, and the whole
paste must arrive byte for byte.
"""

import argparse
import fcntl
import os
import pty
import re
import select
import signal
import struct
import sys
import tempfile
import termios
import threading
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BINARY = os.path.join(REPO, "build", "term-ime")
BLOCK = 4096

READER_SRC = '''
import os, select, sys, time
want, out, secs = int(sys.argv[1]), sys.argv[2], float(sys.argv[3])
deadline = time.time() + secs
buf = b""
while len(buf) < want and time.time() < deadline:
    if not select.select([0], [], [], 0.5)[0]:
        continue
    try:
        chunk = os.read(0, 65536)
    except OSError:
        break
    if not chunk:
        break
    buf += chunk
open(out, "wb").write(buf)
'''


def tagged_blob(n):
    """`n` bytes split into BLOCK-sized runs, each tagged with its own index.

    A repeatable pattern per block, so a splice or a hole shows up as a block
    that does not match its position -- plain 'aaaa...' would only prove bytes
    went through, not that they stayed in order.
    """
    out = bytearray()
    while len(out) < n:
        i = len(out) // BLOCK
        tag = ("%02x" % i).encode()
        out += tag * (BLOCK // 2)
    return bytes(out[:n])


class TermIme:
    """term-ime on a pty this process owns, with a hermetic HOME."""

    def __init__(self, cols=100, rows=40):
        self.tmp = tempfile.mkdtemp(prefix="term-ime-paste-")
        home = os.path.join(self.tmp, "home")
        self.log = os.path.join(home, ".cache", "term-ime", "term-ime.log")
        env = dict(os.environ)
        env["HOME"] = home
        env["XDG_CONFIG_HOME"] = os.path.join(self.tmp, "config")
        env["TERM"] = "xterm-256color"
        # Hermetic, like every other e2e script here: the child shell comes from
        # $SHELL, so leaving it inherited made this test measure the developer's
        # login shell. Under zsh the very first keystrokes get mangled before they
        # reach the prompt (see brain/pages/e2e-harness-contract.md), which failed
        # the MARKER gate and reported "keys never reached a live shell" on a build
        # that passes the paste checks under /bin/sh.
        env["SHELL"] = "/bin/sh"
        for d in (env["HOME"], env["XDG_CONFIG_HOME"]):
            os.makedirs(d, exist_ok=True)

        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.pid = os.fork()
        if self.pid == 0:
            os.dup2(slave, 0)
            os.dup2(slave, 1)
            os.dup2(slave, 2)
            os.close(slave)
            os.execve(BINARY, [BINARY], env)
        os.close(slave)
        self.rx = bytearray()
        self._stop = threading.Event()
        threading.Thread(target=self._drain, daemon=True).start()

    def _drain(self):
        # term-ime forwards shell output to stdout; if this side stops reading,
        # its write to the terminal blocks and the whole app appears to freeze.
        while not self._stop.is_set():
            try:
                if select.select([self.master], [], [], 0.05)[0]:
                    data = os.read(self.master, 65536)
                    if not data:
                        return
                    self.rx.extend(data)
            except OSError:
                return

    def screen(self):
        s = bytes(self.rx).decode("utf-8", "replace")
        s = re.sub(r"\x1b\[[?0-9;]*[a-zA-Z]", "", s)
        return re.sub(r"\x1b\][^\x07]*\x07", "", s)

    def wait_for(self, pattern, timeout=25.0):
        end = time.time() + timeout
        rx = re.compile(pattern)
        while time.time() < end:
            if rx.search(self.screen()):
                return True
            time.sleep(0.1)
        return False

    def send_line(self, line, delay=0.4):
        os.write(self.master, (line + "\n").encode())
        time.sleep(delay)

    def paste(self, blob, chunk=8192, gap=0.01):
        sent = 0
        while sent < len(blob):
            view = memoryview(blob)[sent:sent + chunk]
            while view:
                view = view[os.write(self.master, view):]  # retry short writes
            sent += chunk
            time.sleep(gap)
        return sent

    def close(self):
        self._stop.set()
        try:
            os.kill(self.pid, signal.SIGTERM)
            os.waitpid(self.pid, os.WNOHANG)
        except (ProcessLookupError, ChildProcessError):
            pass
        os.close(self.master)


def dropped_bytes(log_path):
    try:
        text = open(log_path, errors="replace").read()
    except OSError:
        return 0, 0
    dropped = sum(int(m) for m in re.findall(r"dropped (\d+) newest bytes", text))
    events = len(re.findall(r"queue full, dropped", text))
    return dropped, events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bytes", type=int, default=64 * 1024)
    ap.add_argument("--stall", type=int, default=0, help="seconds the reader sleeps before reading")
    args = ap.parse_args()

    if not os.path.exists(BINARY):
        print(f"error: {BINARY} not built")
        return 2

    blob = tagged_blob(args.bytes)
    recv = os.path.join(tempfile.mkdtemp(prefix="term-ime-paste-recv-"), "recv.bin")
    reader = recv + ".py"
    with open(reader, "w") as fh:
        fh.write(READER_SRC)

    app = TermIme()
    rc = 1
    try:
        # librime deploys into the fresh HOME before the first frame; the poll
        # window has to cover it or the run just measures disk latency.
        if not app.wait_for(r"\[EN\]|\[拼\]", timeout=60.0):
            print("FAIL: term-ime never painted a first frame")
            print("screen:", repr(app.screen()[:200]))
            return 1

        app.send_line("echo MARKER_$((40+2))", 1.2)
        if "MARKER_42" not in app.screen():
            print("FAIL: keys never reached a live shell -- nothing below would mean anything")
            return 1

        sleep_for = f"sleep {args.stall}; " if args.stall else ""
        deadline = max(30, args.stall + 30)
        app.send_line(
            f"bash -c 'stty raw -echo; {sleep_for}"
            f"python3 {reader} {len(blob)} {recv} {deadline}; stty sane'",
            2.5,
        )
        sent = app.paste(blob)
        print(f"pasted {sent} bytes ({args.bytes} requested, stall {args.stall}s)")

        if args.stall:
            # Nothing drains the outbound queue by itself -- a retry happens on
            # the next write()/flush(). Keys after the reader wakes up are what
            # a real user does anyway, and they give the queue that trigger.
            time.sleep(args.stall + 3)
            for _ in range(6):
                app.send_line("", 0.6)

        for _ in range(deadline + 10):
            if os.path.exists(recv):
                break
            time.sleep(1)

        got = open(recv, "rb").read() if os.path.exists(recv) else b""
        dropped, events = dropped_bytes(app.log)
        print(f"reader received {len(got)} of {len(blob)}")
        # Empty lines are the drain nudges sent above; the tagged blob contains
        # no newline at all, so anything trailing is ours.
        body = got.rstrip(b"\n")
        nudges = len(got) - len(body)
        prefix = body == blob[:len(body)]
        print(f"received part is a contiguous prefix of the paste (no splice): {prefix}"
              f"{f' [{nudges} nudge line(s) removed]' if nudges else ''}")

        if body == blob:
            print("PASS: delivered complete and in order")
            rc = 0
        elif prefix and args.stall and len(body) >= BLOCK:
            print(f"PASS(order): stalled reader lost the tail -- {events} logged drop events, "
                  f"{dropped} bytes admitted in the log; what arrived stayed in order")
            rc = 0
        elif prefix:
            print("FAIL: the reader took nothing, so 'in order' would be vacuous -- "
                  "the scenario never engaged")
            rc = 1
        else:
            mm = next((i for i, (a, b) in enumerate(zip(body, blob)) if a != b), min(len(body), len(blob)))
            print(f"FAIL: stream spliced, first mismatch at {mm}: got {body[max(0, mm-8):mm+8]!r} "
                  f"want {blob[max(0, mm-8):mm+8]!r}")
            rc = 1
        return rc
    finally:
        app.close()


if __name__ == "__main__":
    sys.exit(main())
