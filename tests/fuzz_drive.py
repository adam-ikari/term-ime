#!/usr/bin/env python3
"""Fuzz driver: random action sequences into a real term-ime PTY.

tests/monkey_sequences.py already models the action space (letters, digits,
Ctrl+A chords, malformed CSI, arrows, resize, waits) but nothing drives it —
it was only ever used interactively through the MCP tools, and it is not in CI.

This imports that model rather than reimplementing it. The invariant checked is
the blunt one that matters most: **the process must survive the sequence**. A
crash or an unexpected exit is a finding regardless of what the user-visible
symptom turns out to be.

WHAT THIS DOES NOT CATCH — read before trusting a clean run:

  - Wrong output. Survival says nothing about whether the right bytes reached the
    shell. The behavioural invariants live in the named e2e suites; this only
    covers "did it fall over".
  - Hangs. A process that wedges but stays alive reads as ok (verified: a
    `sleep 600` stand-in passes). Detecting that needs a liveness probe, which
    this does not do.
  - Anything after the sequence: the app is killed at the end of each round, so
    a crash on shutdown or on the first keystroke after a long idle is invisible.

Validated by mutation, since a survival check that cannot fail is worthless:
pointing BIN at `exit 3` reports "exited at step 0", and pointing it at a
`kill -SEGV` reports "signal 11". A stand-in that merely hangs is reported ok,
which is the known gap above.

Usage:
    python3 tests/fuzz_drive.py [rounds] [steps_per_round] [seed]

Not wired into CI: a round pays ~6s of librime deploy plus 20ms per step, so it
is a tool to run deliberately, not on every push.
"""

import os
import pty
import random
import select
import signal
import struct
import sys
import termios
import fcntl
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import monkey_sequences as ms  # noqa: E402

BIN = str(Path(__file__).resolve().parents[1] / "build" / "term-ime")


def drive(actions, home):
    """Feed `actions` into a fresh term-ime. Returns (status, detail).

    status: "ok" | "died" | "hung"
    """
    pid, master = pty.fork()
    if pid == 0:
        os.environ["HOME"] = home
        os.environ["XDG_CONFIG_HOME"] = os.path.join(home, ".config")
        os.environ["XDG_DATA_HOME"] = os.path.join(home, ".local", "share")
        os.environ["TERM"] = "xterm-256color"
        os.environ["SHELL"] = "/bin/sh"
        os.execv(BIN, [BIN])
        os._exit(127)

    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 120, 0, 0))
    flags = fcntl.fcntl(master, fcntl.F_GETFL)
    fcntl.fcntl(master, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    time.sleep(6.0)  # let librime deploy into the fresh HOME
    try:
        while select.select([master], [], [], 0.0)[0]:
            os.read(master, 65536)
    except OSError:
        pass

    status, detail = "ok", ""
    for i, act in enumerate(actions):
        try:
            wpid, wstatus = os.waitpid(pid, os.WNOHANG)
        except ChildProcessError:
            wpid, wstatus = pid, 0
        if wpid == pid:
            status = "died"
            detail = "exited at step %d (%s)" % (
                i, "signal %d" % WTERMSIG(wstatus) if os.WIFSIGNALED(wstatus)
                else "code %s" % os.WEXITSTATUS(wstatus))
            break

        if isinstance(act, ms.Resize):
            try:
                fcntl.ioctl(master, termios.TIOCSWINSZ,
                            struct.pack("HHHH", act.rows, act.cols, 0, 0))
            except OSError:
                pass
        elif isinstance(act, ms.Wait):
            time.sleep(act.ms / 1000.0)
        else:
            data = act.to_bytes() if hasattr(act, "to_bytes") else b""
            if not data:
                continue
            try:
                os.write(master, data)
            except OSError:
                status, detail = "died", "write failed at step %d" % i
                break
        time.sleep(0.02)
        try:
            while select.select([master], [], [], 0.0)[0]:
                os.read(master, 65536)
        except OSError:
            pass

    # Give it a moment, then check it is still alive and responsive.
    if status == "ok":
        time.sleep(0.5)
        try:
            wpid, _ = os.waitpid(pid, os.WNOHANG)
            if wpid == pid:
                status = "died"
                detail = "exited after the last step"
        except ChildProcessError:
            status, detail = "died", "reaped after the last step"

    try:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    except Exception:
        pass
    try:
        os.close(master)
    except Exception:
        pass
    return status, detail


def WTERMSIG(st):
    return os.WTERMSIG(st) if hasattr(os, "WTERMSIG") else (st & 0x7F)


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 25
    steps = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    base_seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1

    if not Path(BIN).exists():
        print("binary not found:", BIN)
        return 1

    findings = []
    for r in range(rounds):
        seed = base_seed + r
        rng = random.Random(seed)
        bias = "zh" if r % 2 else "en"
        acts = [ms.weighted_action(rng, bias) for _ in range(steps)]
        # Fresh HOME per round: reusing one would let a crashed round poison the
        # next, and a half-deployed dictionary looks like a startup failure.
        home = "/tmp/fuzz_home_%d" % seed
        os.system("rm -rf %s && mkdir -p %s" % (home, home))
        status, detail = drive(acts, home)
        label = "round %d seed=%d bias=%s" % (r, seed, bias)
        if status == "ok":
            print("  ok   %s" % label, flush=True)
        else:
            print("  %-4s %s -- %s" % (status, label, detail), flush=True)
            findings.append((seed, bias, acts, detail))

    print("\n%d/%d rounds clean" % (rounds - len(findings), rounds))
    for seed, bias, acts, detail in findings:
        print("\n=== FINDING seed=%d bias=%s: %s" % (seed, bias, detail))
        print("    " + " ".join(repr(a) for a in acts))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())