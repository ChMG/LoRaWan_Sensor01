#!/usr/bin/env bash
set -euo pipefail

ENV_NAME="${1:-LoRaWan_Sensor01}"
UPLOAD_PORT="${2:-}"

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

PYTHON_BIN="$PROJECT_DIR/.venv/bin/python"
if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "Fehler: Python aus .venv nicht gefunden: $PYTHON_BIN"
  echo "Bitte zuerst virtuelle Umgebung einrichten/aktivieren."
  exit 1
fi

sudo modprobe usbserial || true
sudo modprobe cp210x || true

if [[ -z "$UPLOAD_PORT" ]]; then
  for candidate in /dev/ttyUSB* /dev/ttyACM*; do
    if [[ -e "$candidate" ]]; then
      UPLOAD_PORT="$candidate"
      break
    fi
  done
fi

if [[ -z "$UPLOAD_PORT" ]]; then
  echo "Kein serieller Port gefunden."
  echo "Prüfe USB-Passthrough (usbipd attach --wsl ...) und versuche erneut."
  exit 2
fi

if [[ ! -e "$UPLOAD_PORT" ]]; then
  echo "Port existiert nicht: $UPLOAD_PORT"
  exit 3
fi

sudo chmod 666 "$UPLOAD_PORT" || true

echo "Nutze Environment: $ENV_NAME"
echo "Nutze Upload-Port: $UPLOAD_PORT"

"$PYTHON_BIN" -m platformio run -e "$ENV_NAME" -t upload --upload-port "$UPLOAD_PORT"
