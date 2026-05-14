#!/usr/bin/env python3
"""Test term-ime using tui-debug-mcp MCP tools."""

import asyncio
import sys
sys.path.insert(0, '/home/gem/project/tui-debug-mcp/src')

from tui_debug_mcp import TUIProcess
import re

def clean_ansi(text):
    """Remove ANSI escape sequences."""
    clean = re.sub(r'\x1b\[[^a-zA-Z]*[a-zA-Z]', '', text)
    clean = re.sub(r'\x1b\].*?\x07', '', clean)
    clean = clean.replace('\r', '')
    return clean

async def test_term_ime():
    """Test term-ime using TUIProcess."""
    print("=== TUI Debug Session for term-ime (MCP style) ===\n")

    # Create TUI process
    session = TUIProcess(
        session_id="term-ime-test",
        command="cd /home/gem/project/term-ime/build && ./term-ime",
        rows=24,
        cols=80
    )

    # Start (equivalent to tui_start)
    print("Starting term-ime...")
    if not session.start():
        print("Failed to start term-ime")
        return 1
    print(f"Process started (PID: {session.pid})")

    # Wait for initialization
    await asyncio.sleep(2)

    # Read initial screen (equivalent to tui_read)
    print("\n--- Initial Screen (tui_read) ---")
    screen = session.read_screen(timeout=1.0)
    print(clean_ansi(screen)[:500])
    print(f"\nRaw bytes: {len(screen.encode())}")

    # Check if alive (equivalent to tui_alive)
    print(f"\n--- Process Status (tui_alive) ---")
    print(f"Alive: {session.is_alive()}")

    if not session.is_alive():
        print("Process exited unexpectedly")
        session.terminate()
        return 1

    # Test IME: type 'n' (equivalent to tui_type)
    print("\n--- Testing IME: typing 'n' (tui_type) ---")
    session.send_input('n')
    await asyncio.sleep(1)
    screen = session.read_screen(timeout=0.5)
    print(clean_ansi(screen)[:300])

    # Test IME: type 'i'
    print("\n--- Testing IME: typing 'i' ---")
    session.send_input('i')
    await asyncio.sleep(1)
    screen = session.read_screen(timeout=0.5)
    print(clean_ansi(screen)[:300])

    # Test IME: select candidate '1'
    print("\n--- Testing IME: selecting '1' ---")
    session.send_input('1')
    await asyncio.sleep(1)
    screen = session.read_screen(timeout=0.5)
    print(clean_ansi(screen)[:300])

    # Get history (equivalent to tui_history)
    print("\n--- History (tui_history) ---")
    for i, event in enumerate(session.history[-5:]):
        print(f"{i+1}. {event['type']}: {event['content'][:50]}...")

    # Stop session (equivalent to tui_stop)
    print("\n--- Cleanup (tui_stop) ---")
    session.terminate()
    print("Session terminated")

    return 0

if __name__ == "__main__":
    sys.exit(asyncio.run(test_term_ime()))
