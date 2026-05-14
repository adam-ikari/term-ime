#!/bin/bash
# Debug script for term-ime

BUILD_DIR="${BUILD_DIR:-build}"
EXECUTABLE="$BUILD_DIR/term-ime"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Building..."
    cd build && cmake .. && make
fi

echo "Starting GDB debug session..."
echo "Commands:"
echo "  run       - Start program"
echo "  bt        - Backtrace"
echo "  info reg  - Register info"
echo "  x/10x $sp - Stack dump"
echo ""

gdb -q "$EXECUTABLE"