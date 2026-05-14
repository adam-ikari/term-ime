#!/usr/bin/env python3
"""Debug script for term-ime using TUI debug tools."""

import sys
import os
import pty
import fcntl
import struct
import termios
import select
import time
import signal

def start_tui(command, rows=24, cols=80):
    """Start a TUI process and return (pid, master_fd)."""
    pid, master_fd = pty.fork()

    if pid == 0:
        # Child process - execute command directly
        os.execvp("sh", ["sh", "-c", command])
    else:
        # Parent process
        # Set terminal size
        winsize = struct.pack('HHHH', rows, cols, 0, 0)
        fcntl.ioctl(master_fd, termios.TIOCSWINSZ, winsize)
        # Set non-blocking
        flags = fcntl.fcntl(master_fd, fcntl.F_GETFL)
        fcntl.fcntl(master_fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        return pid, master_fd

def read_output(master_fd, timeout=0.5):
    """Read output from master fd."""
    output = b""
    while True:
        try:
            ready, _, _ = select.select([master_fd], [], [], timeout)
            if not ready:
                break
            data = os.read(master_fd, 65536)
            if data:
                output += data
            else:
                break
        except (OSError, IOError):
            break
    return output.decode("utf-8", errors="replace")

def send_input(master_fd, text):
    """Send input to master fd."""
    os.write(master_fd, text.encode())

def main():
    print("=== TUI Debug Session for term-ime ===\n")

    # Start term-ime from build directory where data/pinyin.dict exists
    print("Starting term-ime...")
    pid, master_fd = start_tui("cd /home/gem/project/term-ime/build && ./term-ime", rows=24, cols=80)
    print(f"Process started (PID: {pid})")

    # Wait for initialization
    time.sleep(2)

    # Read initial screen
    print("\n--- Initial Screen ---")
    screen = read_output(master_fd, timeout=1.0)
    # Filter out escape sequences for display
    import re
    clean = re.sub(r'\x1b\[[^a-zA-Z]*[a-zA-Z]', '', screen)
    clean = re.sub(r'\x1b\].*?\x07', '', clean)
    clean = clean.replace('\r', '')
    print(clean[:500] if clean else "(empty)")
    print(f"\nRaw bytes: {len(screen.encode())} bytes")

    # Check if process is alive
    try:
        ret_pid, status = os.waitpid(pid, os.WNOHANG)
        if ret_pid != 0:
            print(f"\nProcess exited with status: {status}")
            return 1
    except Exception as e:
        print(f"Status check error: {e}")
        return 1

    # Test: send 'n' to start IME input
    print("\n--- Testing IME: typing 'n' ---")
    send_input(master_fd, 'n')
    time.sleep(1)
    screen = read_output(master_fd, timeout=0.5)
    clean = re.sub(r'\x1b\[[^a-zA-Z]*[a-zA-Z]', '', screen)
    clean = clean.replace('\r', '')
    print(clean[:300] if clean else "(no change)")

    # Test: send 'i' to complete "ni"
    print("\n--- Testing IME: typing 'i' ---")
    send_input(master_fd, 'i')
    time.sleep(1)
    screen = read_output(master_fd, timeout=0.5)
    clean = re.sub(r'\x1b\[[^a-zA-Z]*[a-zA-Z]', '', screen)
    clean = clean.replace('\r', '')
    print(clean[:300] if clean else "(no change)")

    # Test: select candidate '1'
    print("\n--- Testing IME: selecting '1' ---")
    send_input(master_fd, '1')
    time.sleep(1)
    screen = read_output(master_fd, timeout=0.5)
    clean = re.sub(r'\x1b\[[^a-zA-Z]*[a-zA-Z]', '', screen)
    clean = clean.replace('\r', '')
    print(clean[:300] if clean else "(no change)")

    # Check process status
    print(f"\n--- Process Status ---")
    try:
        ret_pid, status = os.waitpid(pid, os.WNOHANG)
        print(f"Alive: {ret_pid == 0}")
    except Exception as e:
        print(f"Status check error: {e}")

    # Cleanup
    print("\n--- Cleanup ---")
    try:
        os.kill(pid, signal.SIGTERM)
        os.waitpid(pid, 0)
    except:
        pass
    os.close(master_fd)
    print("Session terminated")

    return 0

if __name__ == "__main__":
    sys.exit(main())
