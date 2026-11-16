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

SRC="$(python3 "$SCRIPT_DIR/../tool/fetch_source.py" opus_decoder)"
cd "$SRC"
mkdir -p build && cd build
if [ ! -f "Makefile" ]; then
  cmake .. \
    -DCMAKE_C_COMPILER="$ZIG_BIN" \
    -DCMAKE_C_COMPILER_ARG1="cc -target wasm32-wasi" \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DOPUS_BUILD_SHARED_LIBRARY=OFF \
    -DOPUS_BUILD_TESTING=OFF \
    -DOPUS_BUILD_PROGRAMS=OFF \
    -DOPUS_DISABLE_INTRINSICS=ON
fi
cmake --build .

cd ../../..
OUT_WASM="$OUTPUT_DIR/opus_frame_decoder.wasm"

$ZIG_BIN cc -target wasm32-wasi -Os \
  -I"$SRC" -I"$SRC/include" \
  -Wl,--no-entry -Wl,--strip-all \
  -Wl,--export-memory \
  opus_decoder.c $(find "$SRC/build/CMakeFiles/opus.dir/" -name "*.obj") \
  -o "$OUT_WASM" -lc -lm

PRE_OPT_SIZE=$(wc -c < "$OUT_WASM")
if command -v wasm-opt >/dev/null 2>&1; then
  wasm-opt -O4 --converge --enable-bulk-memory --enable-sign-ext --enable-mutable-globals --enable-nontrapping-float-to-int --strip-dwarf "$OUT_WASM" -o "$OUT_WASM"
fi
POST_OPT_SIZE=$(wc -c < "$OUT_WASM")
echo "built opus_frame_decoder.wasm (unoptimized: $PRE_OPT_SIZE bytes, optimized: $POST_OPT_SIZE bytes)"
