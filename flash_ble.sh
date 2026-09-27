#!/usr/bin/env bash
set -e

SKETCH_DIR="/home/felix/Projects/TVremote/TVremote_BLE"
FQBN="esp32:esp32:esp32c3"

echo "=== ESP32-C3 BLE TV Remote Flasher ==="

# Check for serial port
PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -n 1 || true)

if [ -z "$PORT" ]; then
    echo "Waiting for ESP32-C3 to be plugged in via USB..."
    echo "(Make sure you are using a USB data cable and not a power-only cable)"
    while [ -z "$PORT" ]; do
        sleep 1
        PORT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -n 1 || true)
    done
fi

echo "Found device at $PORT!"
echo "Flashing TVremote_BLE..."

arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR" --upload -p "$PORT"

echo ""
echo "=========================================="
echo " Flash complete!"
echo " Device is running and broadcasting BLE as:"
echo "       'ESP32C3-TV-Remote'"
echo "=========================================="
