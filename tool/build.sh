#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT/dist}"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
export OUTPUT_DIR
for module in quickjs opus_decoder webp_encoder; do
  sh "$ROOT/$module/build.sh"
done
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/THIRD_PARTY_NOTICES.en.md" "$OUTPUT_DIR/"
cp -R "$ROOT/licenses" "$OUTPUT_DIR/"
python3 "$ROOT/tool/package.py" "$OUTPUT_DIR"
