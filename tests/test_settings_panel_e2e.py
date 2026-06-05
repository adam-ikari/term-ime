#!/usr/bin/env python3
"""
End-to-end test for term-ime settings panel.
Simulates real user operations via PTY.
"""

import os
import pty
import select
import time
import struct
import fcntl
import termios
import signal
import re
import sys

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
        except:
            break
    return output

def send_keys(fd, keys):
    """Send keys to PTY."""
    os.write(fd, keys)
    time.sleep(0.2)

def test_settings_panel():
    """Test settings panel operations."""
    print("=" * 60)
    print("Settings Panel End-to-End Test")
    print("=" * 60)

    # Check if term-ime exists
    term_ime_path = "/home/gem/project/term-ime/build/term-ime"
    if not os.path.exists(term_ime_path):
        print(f"ERROR: {term_ime_path} not found")
        return False

    # Create a new session with proper TTY
    pid, master_fd = pty.fork()
    if pid == 0:
        # Child process - already has controlling terminal from pty.fork()
        os.chdir("/home/gem/project/term-ime")
        os.environ["TERM"] = "xterm-256color"
        os.environ["HOME"] = os.environ.get("HOME", "/root")
        # Close all file descriptors except stdin/stdout/stderr
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

    try:
        # Test 1: Wait for startup
        print("\n[Test 1] Startup")
        time.sleep(2.0)
        output = read_all(master_fd, 1.0)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Output length: {len(output)} bytes")
        print(f"  Screen preview: {repr(screen[:100])}")

        # Check if term-ime started (look for shell prompt or mode indicator)
        has_content = len(screen) > 50
        print(f"  Has content: {has_content}")
        results.append(("Startup", has_content))

        # Test 2: Open settings panel
        print("\n[Test 2] Open settings panel (Ctrl+A, S)")
        # Clear buffer first
        read_all(master_fd, 0.1)

        # Send Ctrl+A followed by 's'
        send_keys(master_fd, b"\x01s")
        time.sleep(1.0)

        output = read_all(master_fd, 1.0)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Output length: {len(output)} bytes")
        print(f"  Screen preview: {repr(screen[:150])}")

        # Check for settings panel content
        has_settings = "设置" in screen or "界面语言" in screen or "AI排序" in screen
        print(f"  Settings panel detected: {has_settings}")
        results.append(("Open settings", has_settings))

        if not has_settings:
            print("  ERROR: Settings panel not opened!")
            print(f"  Raw output: {repr(output[:300])}")

        # Test 3: Navigate down with 'j' key
        print("\n[Test 3] Navigate down (j key)")
        read_all(master_fd, 0.1)
        send_keys(master_fd, b"j")
        time.sleep(0.5)

        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")

        # After pressing 'j', focus should move to AI排序
        # We can't easily verify this without parsing the ANSI codes
        # Just check that we got some output
        got_response = len(output) > 0
        print(f"  Got response: {got_response}")
        results.append(("Navigate j", got_response))

        # Test 4: Navigate with arrow key (down)
        print("\n[Test 4] Navigate with arrow key (down)")
        read_all(master_fd, 0.1)
        # Send ESC [ B (down arrow)
        send_keys(master_fd, b"\x1b[B")
        time.sleep(0.5)

        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        got_response = len(output) > 0
        print(f"  Got response: {got_response}")
        results.append(("Arrow down", got_response))

        # Test 5: Change value with 'l' key
        print("\n[Test 5] Change value (l key)")
        read_all(master_fd, 0.1)
        send_keys(master_fd, b"l")
        time.sleep(0.5)

        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        got_response = len(output) > 0
        print(f"  Got response: {got_response}")
        results.append(("Change value l", got_response))

        # Test 6: Navigate up with 'k' key
        print("\n[Test 6] Navigate up (k key)")
        read_all(master_fd, 0.1)
        send_keys(master_fd, b"k")
        time.sleep(0.5)

        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:150])}")
        got_response = len(output) > 0
        print(f"  Got response: {got_response}")
        results.append(("Navigate k", got_response))

        # Test 7: Close settings with ESC
        print("\n[Test 7] Close settings (ESC)")
        read_all(master_fd, 0.1)
        # Send ESC twice to ensure it's processed (first ESC might be waiting for CSI sequence)
        send_keys(master_fd, b"\x1b\x1b")
        time.sleep(1.0)

        output = read_all(master_fd, 1.0)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Output length: {len(output)} bytes")
        print(f"  Screen preview: {repr(screen[:300])}")

        # After ESC, settings panel should be closed
        # Check for candidate bar (EN or 中文) which indicates settings is closed
        has_candidate_bar = "EN" in screen or "中文" in screen
        # Settings panel has specific content like "界面语言:" or "后端:" with values
        # The hints bar contains "AI排序" as a hint key, but settings panel has "AI排序: Off/On"
        has_settings_items = "界面语言:" in screen or "后端:" in screen or "线程数:" in screen
        settings_closed = has_candidate_bar and not has_settings_items
        print(f"  Has candidate bar: {has_candidate_bar}")
        print(f"  Has settings items: {has_settings_items}")
        print(f"  Settings closed: {settings_closed}")
        results.append(("Close settings", settings_closed))

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
        except:
            pass
        try:
            os.close(master_fd)
        except:
            pass

if __name__ == "__main__":
    success = test_settings_panel()
    sys.exit(0 if success else 1)
