#!/usr/bin/env python3
"""Test settings panel operation in actual term-ime."""

import os
import pty
import select
import time
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

def read_all(fd, timeout=1.0):
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

def test_settings():
    print("=== Settings Panel E2E Test ===\n")

    pid, master_fd = pty.fork()
    if pid == 0:
        os.setsid()
        os.chdir("/home/gem/project/term-ime")
        os.execvp("./build/term-ime", ["./build/term-ime"])

    # Set terminal size
    winsize = struct.pack('HHHH', 24, 80, 0, 0)
    fcntl.ioctl(master_fd, termios.TIOCSWINSZ, winsize)

    # Non-blocking
    flags = fcntl.fcntl(master_fd, fcntl.F_GETFL)
    fcntl.fcntl(master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    results = []

    try:
        # Wait for startup
        print("Test 1: Startup")
        time.sleep(2.0)
        output = read_all(master_fd, 1.0)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen length: {len(screen)}")
        has_mode = "EN" in screen or "中文" in screen
        print(f"  Mode indicator: {has_mode}")
        results.append(has_mode)

        # Open settings panel
        print("\nTest 2: Open settings (Ctrl+A, S)")
        os.write(master_fd, b"\x01s")  # Ctrl+A + S
        time.sleep(1.0)
        output = read_all(master_fd, 1.0)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {screen[:200]}")
        has_settings = "设置" in screen or "界面语言" in screen
        print(f"  Settings panel visible: {has_settings}")
        results.append(has_settings)

        if not has_settings:
            print("  ERROR: Settings panel not opened, aborting")
            return

        # Navigate down with 'j'
        print("\nTest 3: Navigate down (j key)")
        os.write(master_fd, b"j")
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        # Check if focus moved - should see AI排序 highlighted
        has_ai_focus = "AI排序" in screen and ("[" in screen or screen.find("AI排序") < screen.find("界面语言"))
        print(f"  AI排序 focused: {has_ai_focus}")
        results.append(has_ai_focus)

        # Navigate down again with arrow key (ESC[B)
        print("\nTest 4: Navigate down (arrow key)")
        os.write(master_fd, b"\x1b[B")  # Down arrow
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        has_backend_focus = "后端" in screen
        print(f"  后端 focused: {has_backend_focus}")
        results.append(has_backend_focus)

        # Change value with 'l' (right)
        print("\nTest 5: Change value (l key)")
        os.write(master_fd, b"l")
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        print(f"  Screen preview: {screen[:200]}")
        # Should see CUDA or Metal instead of CPU
        has_cuda = "CUDA" in screen or "Metal" in screen or "Vulkan" in screen
        print(f"  Backend changed: {has_cuda}")
        results.append(has_cuda)

        # Close settings with ESC
        print("\nTest 6: Close settings (ESC)")
        os.write(master_fd, b"\x1b")
        time.sleep(0.5)
        output = read_all(master_fd, 0.5)
        screen = clean_ansi(output.decode('utf-8', errors='replace'))
        settings_closed = "设置" not in screen
        print(f"  Settings closed: {settings_closed}")
        results.append(settings_closed)

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
    test_settings()