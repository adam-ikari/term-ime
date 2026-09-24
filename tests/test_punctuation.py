#!/usr/bin/env python3
"""中文模式标点契约：标点键交给 librime punctuator，提交全角形式。

回归对象：luna_pinyin_simp / _fuzzy 缺少 `punctuator:` 段，编译后的 schema
没有 punct 映射，`process_key` 直接拒绝 `,` → 终端只收到半角 ','，全角
中文标点不可用。

契约：
  1. 中文模式下，半角标点键提交对应全角标点（',' → '，'，'?' → '？'）。
  2. 标点被提交而非组成候选（不残留未提交的 composition）。
  3. ASCII 模式下标点原样透传给 shell。
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

ANSI = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
OSC = re.compile(r"\x1b\][^\x07]*\x07")

# (半角键, 期望的全角标点)
PUNCT_PROBES = [
    (b",", "，"),
    (b".", "。"),
    (b"?", "？"),
    (b"!", "！"),
    (b":", "："),
    (b";", "；"),
]

RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append((name, bool(ok)))
    print("  [%s] %-52s %s" % ("PASS" if ok else "FAIL", name, detail[:70]), flush=True)


def strip_ansi(data: bytes) -> str:
    return OSC.sub("", ANSI.sub("", data.decode("utf-8", "replace")))


class Session:
    """一次性 HOME 下启动真实二进制，PTY 驱动。"""

    def __init__(self) -> None:
        self.home = tempfile.mkdtemp(prefix="pu-")
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
        if not s.wait_for(r"\$\s*$", seconds=60.0):
            check("shell ready", False)
            return 1
        s.read(0.5)

        # 中文模式。就绪门已在上面：此刻 shell 已响应，模式切换不会被丢弃。
        s.send(b"\x01 ")
        if not s.wait_for(r"\[拼\]", seconds=8.0):
            check("chinese mode", False)
            return 1
        s.read(0.6)

        # 1+2：每个标点键都提交对应全角标点。
        for key, expect in PUNCT_PROBES:
            s.read(0.2)
            s.send(key)
            time.sleep(0.5)
            got = strip_ansi(s.read(0.8))
            check("chinese %r commits %s" % (key.decode(), expect), expect in got, got[-40:])

    finally:
        s.close()

    # 3：ASCII 模式标点原样透传。独立会话，避免中文会话里累积在 shell
    # 命令行上的全角标点污染断言。
    s = Session()
    try:
        if not s.wait_for(r"\[EN\]", seconds=60.0):
            check("ascii: ready", False)
            return 1
        s.read(0.6)
        # 探针式断言：输入行与求值行不同，只有活着的 shell 才会打印结果。
        # 半角逗号必须原样进入命令，而非被转成全角。
        s.send(b"echo ASC$((6*7))-,\r")
        ok = s.wait_for(r"ASC42-,", seconds=10.0)
        check("ascii ',' passes through", ok)
    finally:
        s.close()

    total = len(RESULTS)
    passed = sum(1 for _, ok in RESULTS if ok)
    print("\n===== %d/%d PASS =====" % (passed, total))
    for name, ok in RESULTS:
        if not ok:
            print("  FAILED:", name)

    # Fallback 目录（两套 schema）都必须带上 punct 映射：编译后 schema 缺
    # punctuator 段是本 bug 的根因，直接对产物断言。
    for schema in ("luna_pinyin_simp", "luna_pinyin_simp_fuzzy"):
        path = os.path.join(os.path.dirname(BIN), "share", "rime-data", schema + ".schema.yaml")
        with open(path, encoding="utf-8") as f:
            has = "punctuator:" in f.read()
        check("source schema %s has punctuator" % schema, has, path)

    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
