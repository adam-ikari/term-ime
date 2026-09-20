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
            json.dump({"fuzzy_groups": groups}, f)
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


def run_group_case(groups, label):
    """以指定组集合启动，逐组探针：开→候选含专属字，关→不含。"""
    s = Session(groups)
    try:
        if not s.wait_for(r"\[EN\]|\[拼\]"):
            check(f"{label}: ready", False)
            return
        s.send(b"\x01 ")
        s.wait_for(r"\[拼\]", seconds=8.0)
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

    # 动态组合 schema 物化：部分开启时用户目录应有生成的 schema + prism
    s = Session(["n_l", "nose"])
    try:
        ok = s.wait_for(r"\[EN\]|\[拼\]")
        check("subset: ready", ok)
        udir = os.path.join(s.home, ".local", "share", "term-ime")
        gen_schema = os.path.join(udir, "luna_pinyin_simp_fuzzy_n_l_nose.schema.yaml")
        gen_prism = os.path.join(udir, "build", "luna_pinyin_simp_fuzzy_n_l_nose.prism.bin")
        check("subset: generated schema written", os.path.exists(gen_schema), gen_schema)
        check("subset: prism deployed", os.path.exists(gen_prism), gen_prism)
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
