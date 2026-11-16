#!/bin/sh
set -eu
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/../dist}"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
cd "$SCRIPT_DIR"

mkdir -p .build && cd .build

if command -v zig >/dev/null 2>&1; then
  ZIG_BIN="zig"
else
  echo "zig compiler is required" >&2
  exit 1
fi

SOURCE_DIR="$(python3 "$SCRIPT_DIR/../tool/fetch_source.py" webp_encoder)"
SRC="$SOURCE_DIR/src"

# 排除未使用的解码、图像质量统计及平台专用优化源码
DEC_ONLY="
dsp/dec_mips32.c dsp/dec_mips_dsp_r2.c dsp/dec_msa.c
dsp/dec_neon.c dsp/dec_sse2.c dsp/dec_sse41.c
dsp/upsampling_mips_dsp_r2.c dsp/upsampling_msa.c dsp/upsampling_neon.c
dsp/upsampling_sse2.c dsp/upsampling_sse41.c dsp/ssim_sse2.c
dsp/lossless_mips_dsp_r2.c dsp/lossless_msa.c dsp/lossless_neon.c
dsp/lossless_sse2.c dsp/lossless_sse41.c
dsp/yuv_mips32.c dsp/yuv_mips_dsp_r2.c dsp/yuv_neon.c dsp/yuv_sse2.c dsp/yuv_sse41.c
enc/picture_psnr_enc.c
utils/bit_reader_utils.c utils/huffman_utils.c utils/quant_levels_dec_utils.c
"

rm -f obj_*.o
for f in "$SRC"/enc/*.c "$SRC"/dsp/*.c "$SRC"/utils/*.c "$SOURCE_DIR"/sharpyuv/*.c; do
  rel="${f#$SRC/}"
  skip=0
  for d in $DEC_ONLY; do [ "$rel" = "$d" ] && skip=1; done
  [ $skip = 1 ] && continue
  $ZIG_BIN cc -target wasm32-wasi -Os -flto -DNDEBUG -I"$SOURCE_DIR" -I"$SRC" -c "$f" -o "obj_$(basename "$f" .c).o"
done
$ZIG_BIN cc -target wasm32-wasi -Os -flto -DNDEBUG -I"$SOURCE_DIR" -I"$SRC" -c ../webp_encoder.c -o obj_wrapper.o
OUT_WASM="$OUTPUT_DIR/webp_encoder.wasm"
$ZIG_BIN cc -target wasm32-wasi -Os \
  -Wl,--no-entry -Wl,--strip-all \
  -Wl,--export=webp_alloc -Wl,--export=webp_free \
  -Wl,--export=webp_encode_rgba -Wl,--export=webp_get_output -Wl,--export-memory \
  obj_*.o -o "$OUT_WASM" -lc -lm

PRE_OPT_SIZE=$(wc -c < "$OUT_WASM")
if command -v wasm-opt >/dev/null 2>&1; then
  wasm-opt -O4 --converge --enable-bulk-memory --enable-sign-ext --enable-mutable-globals --enable-nontrapping-float-to-int --strip-dwarf "$OUT_WASM" -o "$OUT_WASM"
fi
POST_OPT_SIZE=$(wc -c < "$OUT_WASM")
echo "built webp_encoder.wasm (unoptimized: $PRE_OPT_SIZE bytes, optimized: $POST_OPT_SIZE bytes)"
