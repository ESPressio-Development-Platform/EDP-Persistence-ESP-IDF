#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${EDP_PERSISTENCE_DEPS_ROOT:-$(cd "$ROOT/.." && pwd)}"
CC="${1:-g++}"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
"$CC" -std=c++20 -Wall -Wextra -Wpedantic -Werror -pthread \
  -I"$ROOT/tests/fakes" -I"$ROOT/src" \
  -I"$DEPS/EDP-Persistence/src" -I"$DEPS/EDP-Memory/src" \
  -I"$DEPS/EDP-System/src" -I"$DEPS/EDP-Platform/src" \
  -I"$DEPS/EDP-Platform-Portable/src" -I"$DEPS/EDP-BoundedTopology/src" \
  -I"$DEPS/EDP-BoundedTypes/src" \
  "$ROOT/tests/LittleFsVfsMountTests.cpp" -o "$OUT"
"$OUT"
echo "LittleFS mount/binding host contracts PASS ($CC)"
