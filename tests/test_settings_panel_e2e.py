#!/usr/bin/env python3
"""
End-to-end test for term-ime settings panel.
Simulates real user operations via PTY.
"""

import os
import sys
import pty
import select
import time
import struct
import fcntl
import termios
import signal
import re
import shutil
import tempfile

def clean_ansi(text):
    """Remove ANSI escape sequences."""
    text = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', text)
    text = re.sub(r'\x1b\].*?\x07', '', text)
    text = re.sub(r'\x1b\[\?[0-9;]*[a-zA-Z]', '', text)
    return text

def read_all(fd, timeout=0.5):
    """Read all available data with timeout."""
    output = b""
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if not ready:
                break
            data = os.read(fd, 4096)
            if data:
                output += data
        except OSError:
            break
    return output

def poll_until(fd, buf, pattern, timeout=10.0):
    """Read from fd into buf until the cleaned text matches pattern, or timeout.

    Returns (matched, buf).
    """
    rx = re.compile(pattern) if isinstance(pattern, str) else pattern
    end_time = time.time() + timeout
    while time.time() < end_time:
        try:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                data = os.read(fd, 4096)
                if not data:
                    break
                buf += data
        except OSError:
            break
        if rx.search(clean_ansi(buf.decode('utf-8', errors='replace'))):
            return True, buf
    return False, buf

def send(fd, data):
    """Write to the pty, ignoring errors once the session is dead."""
    try:
        os.write(fd, data)
    except OSError:
        pass

def test_settings_panel():
    """Test settings panel operations."""
    print("=" * 60)
    print("Settings Panel End-to-End Test")
    print("=" * 60)

    term_ime_path = "/home/gem/project/term-ime/build/term-ime"
    if not os.path.exists(term_ime_path):
        print(f"ERROR: {term_ime_path} not found")
        return False

    # Hermetic environment: keep config/logs out of the real HOME.
    tmp_home = tempfile.mkdtemp(prefix="term-ime-panel-e2e-")
    home = os.path.join(tmp_home, "home")
    config_home = os.path.join(tmp_home, "config")
    os.makedirs(home, exist_ok=True)
    os.makedirs(config_home, exist_ok=True)
    os.environ["HOME"] = home
    os.environ["XDG_CONFIG_HOME"] = config_home
    os.environ["TERM"] = "xterm-256color"

    # Create a new session with proper TTY
    pid, master_fd = pty.fork()
    if pid == 0:
        # Child process - pty.fork() already made us a session leader,
        # so no os.setsid() here (it would fail with EPERM).
        os.chdir("/home/gem/project/term-ime")
        os.execvp(term_ime_path, [term_ime_path])
        # If exec fails
        print(f"Failed to exec {term_ime_path}", file=sys.stderr)
        os._exit(1)

    # Set terminal size
    winsize = struct.pack('HHHH', 24, 80, 0, 0)
    fcntl.ioctl(master_fd, termios.TIOCSWINSZ, winsize)

    # Non-blocking
    flags = fcntl.fcntl(master_fd, fcntl.F_GETFL)
    fcntl.fcntl(master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    results = []

    def drain(fd):
        """Discard unread output left over from a previous frame."""
        read_all(fd, 0.2)

    def row_focus_re(label):
        """Regex matching a focused value row: 'label: [value] < n/m'."""
        return re.compile(re.escape(label) + r':\s*\[([^\]]*)\]\s*<\s*(\d+)/(\d+)')

    try:
        # Test 1: Startup. First frame is painted only after librime deploys
        # into the fresh HOME, so poll for the status bar.
        print("\n[Test 1] Startup")
        ok, buf = poll_until(master_fd, b"", r'\[EN\]', timeout=30.0)
        screen = clean_ansi(buf.decode('utf-8', errors='replace'))
        print(f"  Output length: {len(buf)} bytes")
        print(f"  Screen preview: {repr(screen[:100])}")
        has_status = ok and '[EN]' in screen
        print(f"  Status bar [EN] visible: {has_status}")
        results.append(("Startup", has_status))

        # Test 2: Open settings panel. '界面语言' only exists inside the panel;
        # the status bar hint '^A S 设置' makes a bare '设置' ambiguous.
        # Retried a few times: a keystroke that lands while the app is still
        # starting up can be swallowed, and a retry pair (close + open) always
        # converges back to the open state.
        print("\n[Test 2] Open settings panel (Ctrl+A, S)")
        has_panel = False
        for attempt in range(3):
            drain(master_fd)
            send(master_fd, b"\x01s")  # Ctrl+A + S
            ok, buf = poll_until(master_fd, b"", r'界面语言', timeout=10.0)
            screen = clean_ansi(buf.decode('utf-8', errors='replace'))
            has_panel = ok and '界面语言' in screen and '关闭' in screen
            print(f"  Attempt {attempt + 1}: panel detected: {has_panel}")
            if has_panel:
                break
        print(f"  Output length: {len(buf)} bytes")
        print(f"  Screen preview: {repr(screen[:150])}")
        results.append(("Open settings", has_panel))

        if not has_panel:
            print("  ERROR: Settings panel not opened!")
            print(f"  Raw output: {repr(buf[:300])}")
            print("\n" + "=" * 60)
            print("RESULTS SUMMARY")
            print("=" * 60)
            for name, r in results:
                print(f"  [{'PASS' if r else 'FAIL'}] {name}")
            print(f"\nTotal: {sum(1 for _, r in results if r)}/{len(results)} passed")
            return False

        # Test 3: Navigate down with 'j'. Focus moves from the ui-language
        # row to the close item, which is rendered as '> 关闭 <' when focused.
        print("\n[Test 3] Navigate down (j key)")
        drain(master_fd)
        send(master_fd, b"j")
        ok, buf = poll_until(master_fd, b"", r'>\s*关闭\s*<', timeout=5.0)
        screen = clean_ansi(buf.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        close_focused = ok and bool(re.search(r'>\s*关闭\s*<', screen))
        # The ui row must no longer be the focused one (no brackets).
        ui_unfocused = '界面语言:' in screen and '界面语言: [' not in screen
        print(f"  Close item focused: {close_focused}, ui row unfocused: {ui_unfocused}")
        results.append(("Navigate j", close_focused and ui_unfocused))

        # Test 4: Navigate with arrow key (down). Focus wraps back to the
        # ui-language row, rendered focused again.
        print("\n[Test 4] Navigate with arrow key (down)")
        drain(master_fd)
        send(master_fd, b"\x1b[B")  # ESC [ B (down arrow)
        ok, buf = poll_until(master_fd, b"", row_focus_re('界面语言'), timeout=5.0)
        screen = clean_ansi(buf.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        m = row_focus_re('界面语言').search(screen)
        arrow_ok = ok and bool(m) and m.group(1) == '简体中文'
        close_unfocused = '> 关闭 <' not in screen
        print(f"  Ui row refocused: {arrow_ok}, close unfocused: {close_unfocused}")
        results.append(("Arrow down", arrow_ok and close_unfocused))

        # Test 5: Change value with 'h' (left). zh-CN -> en; on_change
        # re-labels the panel in English.
        print("\n[Test 5] Change value (h key)")
        drain(master_fd)
        send(master_fd, b"h")
        ok, buf = poll_until(master_fd, b"", row_focus_re('UI Language'), timeout=10.0)
        screen = clean_ansi(buf.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        m = row_focus_re('UI Language').search(screen)
        changed = ok and bool(m) and m.group(1) == 'English' and m.group(2) == '1'
        print(f"  Value switched to English: {changed}")
        results.append(("Change value h", changed))

        # Test 6: Navigate up with 'k'. Focus moves to the close item, now
        # rendered in English as '> Close <'.
        print("\n[Test 6] Navigate up (k key)")
        drain(master_fd)
        send(master_fd, b"k")
        ok, buf = poll_until(master_fd, b"", r'>\s*Close\s*<', timeout=5.0)
        screen = clean_ansi(buf.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        close_up = ok and bool(re.search(r'>\s*Close\s*<', screen))
        print(f"  Close item refocused: {close_up}")
        results.append(("Navigate k", close_up))

        # Test 7: Close settings with ESC. ESC closes the panel and redraws
        # the shell view. The panel content must be gone and the shell prompt
        # visible again; the changed setting must also be persisted.
        print("\n[Test 7] Close settings (ESC)")
        drain(master_fd)
        send(master_fd, b"\x1b")
        ok, buf = poll_until(master_fd, b"", r'term-ime', timeout=10.0)
        screen = clean_ansi(buf.decode('utf-8', errors='replace'))
        print(f"  Output length: {len(buf)} bytes")
        print(f"  Screen preview: {repr(screen[:200])}")
        panel_gone = ('Up/Down' not in screen
                      and 'UI Language' not in screen
                      and '界面语言' not in screen
                      and 'Close' not in screen)
        shell_back = ok and 'term-ime' in screen
        config_file = os.path.join(config_home, "term-ime", "config.json")
        persisted = False
        end_time = time.time() + 5.0
        while time.time() < end_time:
            try:
                with open(config_file) as f:
                    saved = f.read()
                if '"ui_language": "en"' in saved:
                    persisted = True
                    break
            except OSError:
                pass
            time.sleep(0.1)
        closed = shell_back and panel_gone and persisted
        print(f"  Shell view back: {shell_back}, panel gone: {panel_gone}, "
              f"config persisted: {persisted}")
        results.append(("Close settings", closed))

        # Summary
        print("\n" + "=" * 60)
        print("RESULTS SUMMARY")
        print("=" * 60)
        passed = sum(1 for _, r in results if r)
        total = len(results)
        for name, result in results:
            status = "PASS" if result else "FAIL"
            print(f"  [{status}] {name}")
        print(f"\nTotal: {passed}/{total} passed")

        return passed == total

    finally:
        try:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        except OSError:
            pass
        try:
            os.close(master_fd)
        except OSError:
            pass
        shutil.rmtree(tmp_home, ignore_errors=True)

if __name__ == "__main__":
    success = test_settings_panel()
    sys.exit(0 if success else 1)
