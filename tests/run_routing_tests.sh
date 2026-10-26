#!/usr/bin/env bash
# Prueba el motor de rutas del ESP32 en el host (sin placas).
# Uso: tests/run_routing_tests.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${TMPDIR:-/tmp}/router_test"

g++ -std=c++17 -Wall -Wextra -O1 \
  -I "$ROOT/esp32_wifi_bridge" \
  "$ROOT/tests/router_test.cpp" \
  "$ROOT/esp32_wifi_bridge/router.cpp" \
  "$ROOT/esp32_wifi_bridge/graph.cpp" \
  -o "$BIN"

"$BIN"
