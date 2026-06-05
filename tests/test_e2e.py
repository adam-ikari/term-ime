#!/usr/bin/env python3
"""Quick end-to-end test for term-ime."""

import os
import sys
import time
import pty
import select
import struct
import fcntl
import termios
import signal
import re

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

def test_quick():
    print("=== Quick E2E Test ===\n")

    term_ime_path = './build/term-ime'

    if not os.path.exists(term_ime_path):
        print(f"Error: {term_ime_path} not found")
        print("Run: make -j$(nproc)")
        return

    pid, master_fd = pty.fork()
    if pid == 0:
        # Child process - start new session
        os.setsid()
        os.execvp(term_ime_path, [term_ime_path])

    # Set terminal size
    winsize = struct.pack('HHHH', 24, 80, 0, 0)
    fcntl.ioctl(master_fd, termios.TIOCSWINSZ, winsize)

    # Non-blocking
    flags = fcntl.fcntl(master_fd, fcntl.F_GETFL)
    fcntl.fcntl(master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    results = []

    try:
        # Test 1: Check startup
        print("Test 1: Startup")
        time.sleep(1.0)  # Wait for app to initialize
        output = read_all(master_fd, 1.0)

        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen length: {len(screen)} chars")
        print(f"  Screen preview: {repr(screen[:100])}")

        # Check for mode indicator (EN or 中文)
        if "EN" in screen or "中文" in screen:
            print("  ✓ Mode indicator found")
            results.append(True)
        else:
            print("  ✗ No mode indicator")
            results.append(False)

        # Test 2: Toggle to Chinese mode
        print("\nTest 2: Toggle to Chinese mode (Ctrl+A, Space)")
        os.write(master_fd, b"\x01 ")  # Ctrl+A + Space
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)

        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        mode = '中文' if '中文' in screen else 'EN'
        print(f"  Mode: {mode}")
        results.append('中文' in screen)

        # Test 3: Input pinyin
        print("\nTest 3: Pinyin input 'nihao'")
        os.write(master_fd, b"nihao")
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)

        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:100])}")

        # Check for candidates or pinyin display
        if "你" in screen or "你好" in screen or "nihao" in screen.lower():
            print("  ✓ Input recognized")
            results.append(True)
        else:
            print("  ✗ No input recognized")
            results.append(False)

        # Cancel with ESC
        os.write(master_fd, b"\x1b")
        time.sleep(0.2)
        read_all(master_fd, 0.2)

        # Test 4: Toggle back to English
        print("\nTest 4: Toggle to English mode (Ctrl+A, Space)")
        os.write(master_fd, b"\x01 ")  # Ctrl+A + Space
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)

        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        mode = 'EN' if 'EN' in screen else '中文'
        print(f"  Mode: {mode}")
        results.append('EN' in screen)

        # Test 5: Settings panel
        print("\nTest 5: Settings panel (Ctrl+A, S)")
        os.write(master_fd, b"\x01s")  # Ctrl+A + S
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)

        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {repr(screen[:100])}")

        # Check for settings panel indicators
        if "设置" in screen or "Settings" in screen or "界面语言" in screen or "AI排序" in screen:
            print("  ✓ Settings panel opened")
            results.append(True)
        else:
            print("  ✗ Settings not detected")
            results.append(False)

        print(f"\n=== Results: {sum(results)}/{len(results)} passed ===")

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
    test_quick()
