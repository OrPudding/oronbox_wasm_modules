#!/bin/sh
set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/../dist}"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
cd "$SCRIPT_DIR"

if command -v zig >/dev/null 2>&1; then
  ZIG_BIN="zig"
else
  echo "zig compiler is required" >&2
  exit 1
fi

SRC="$(python3 "$SCRIPT_DIR/../tool/fetch_source.py" quickjs)"
OUT_WASM="$OUTPUT_DIR/quickjs.wasm"

$ZIG_BIN cc -target wasm32-wasi -Os -flto \
  -D_GNU_SOURCE \
  -D_WASI_EMULATED_PROCESS_CLOCKS \
  -D_WASI_EMULATED_SIGNAL \
  -I"$SRC" \
  -Wl,--no-entry -Wl,--strip-all \
  -Wl,--export-memory \
  -Wl,-z,stack-size=2097152 \
  quickjs_bridge.c \
  "$SRC/quickjs.c" \
  "$SRC/libregexp.c" \
  "$SRC/libunicode.c" \
  "$SRC/dtoa.c" \
  "$SRC/quickjs-libc.c" \
  -o "$OUT_WASM" -lc -lm

PRE_OPT_SIZE=$(wc -c < "$OUT_WASM")
if command -v wasm-opt >/dev/null 2>&1; then
  wasm-opt -O4 --converge \
    --enable-bulk-memory \
    --enable-sign-ext \
    --enable-mutable-globals \
    --enable-nontrapping-float-to-int \
    --strip-dwarf "$OUT_WASM" -o "$OUT_WASM"
fi
POST_OPT_SIZE=$(wc -c < "$OUT_WASM")
echo "built quickjs.wasm (unoptimized: $PRE_OPT_SIZE bytes, optimized: $POST_OPT_SIZE bytes)"
