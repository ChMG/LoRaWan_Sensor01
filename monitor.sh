#!/usr/bin/env bash
set -euo pipefail

MONITOR_BAUD="${1:-115200}"
MONITOR_PORT="${2:-}"

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

PYTHON_BIN="$PROJECT_DIR/.venv/bin/python"
if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "Fehler: Python aus .venv nicht gefunden: $PYTHON_BIN"
  exit 1
fi

sudo modprobe usbserial || true
sudo modprobe cp210x || true

if [[ -z "$MONITOR_PORT" ]]; then
  for candidate in /dev/ttyUSB* /dev/ttyACM*; do
    if [[ -e "$candidate" ]]; then
      MONITOR_PORT="$candidate"
      break
    fi
  done
fi

if [[ -z "$MONITOR_PORT" ]]; then
  echo "Kein serieller Port gefunden."
  echo "Prüfe USB-Passthrough (usbipd attach --wsl ...) und versuche erneut."
  exit 2
fi

if [[ ! -e "$MONITOR_PORT" ]]; then
  echo "Port existiert nicht: $MONITOR_PORT"
  exit 3
fi

sudo chmod 666 "$MONITOR_PORT" || true

echo "Nutze Monitor-Port: $MONITOR_PORT"
echo "Baudrate: $MONITOR_BAUD"

"$PYTHON_BIN" -m platformio device monitor -p "$MONITOR_PORT" -b "$MONITOR_BAUD"
