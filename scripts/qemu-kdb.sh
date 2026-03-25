#!/bin/bash
# QEMU wrapper script that enables KDB activation for semihosting
# Copyright (c) 2026 The F9 Microkernel Project

# Usage: ./scripts/qemu-kdb.sh [ELF_FILE]
#
# Press Ctrl+A then 'k' to trigger KDB
# Press Ctrl+A then 'x' to exit QEMU

ELF_FILE="${1:-build/b-l475e-iot01a/f9.elf}"

if [ ! -f "$ELF_FILE" ]; then
	echo "Error: ELF file not found: $ELF_FILE"
	exit 1
fi

# Create a named pipe for input
PIPE="/tmp/qemu-kdb-$$"
mkfifo "$PIPE" || exit 1

# Cleanup on exit
trap "exec 3>&- 2>/dev/null; rm -f $PIPE" EXIT

echo "==================================================="
echo "F9 Microkernel - QEMU with KDB Support"
echo "==================================================="
echo "ELF: $ELF_FILE"
echo ""
echo "Keyboard shortcuts:"
echo "  Ctrl+A k - Trigger KDB (send '?')"
echo "  Ctrl+A x - Exit QEMU"
echo "  Ctrl+C   - Stop this script"
echo "==================================================="
echo ""

# Hold the FIFO open for writing so QEMU's read side doesn't block on open.
exec 3>"$PIPE"

# Start QEMU with stdin from pipe
qemu-system-arm \
	-M b-l475e-iot01a \
	-nographic \
	-semihosting \
	-serial mon:stdio \
	-kernel "$ELF_FILE" <"$PIPE" &

QEMU_PID=$!

# Function to send character to QEMU
send_kdb() {
	echo "?" >&3
	echo "[KDB] Sent '?' to activate debugger"
}

# Handle Ctrl+C
trap "kill $QEMU_PID 2>/dev/null; exit 0" INT

# Wait for QEMU and handle keyboard shortcuts
while kill -0 $QEMU_PID 2>/dev/null; do
	# Read a single character with timeout
	read -t 0.1 -n 1 -s key

	if [ "$key" = $'\x01' ]; then # Ctrl+A
		read -t 0.5 -n 1 -s key2
		case "$key2" in
		k) send_kdb ;;
		x)
			echo "[QEMU] Exiting..."
			kill $QEMU_PID
			break
			;;
		esac
	fi
done

wait $QEMU_PID 2>/dev/null
exit $?
