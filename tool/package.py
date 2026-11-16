#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import sys
import re
import shutil
import subprocess
import zipfile

def read_unsigned(data, offset):
    value = 0
    for shift in range(0, 35, 7):
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7f) << shift
        if byte < 0x80:
            return value, offset
    raise ValueError("Invalid Wasm integer")


def exports(data):
    offset = 8
    while offset < len(data):
        section = data[offset]
        size, offset = read_unsigned(data, offset + 1)
        end = offset + size
        if end > len(data):
            raise ValueError("Truncated Wasm section")
        if section == 7:
            count, offset = read_unsigned(data, offset)
            names = set()
            for _ in range(count):
                length, offset = read_unsigned(data, offset)
                names.add(data[offset:offset + length].decode())
                offset += length + 1
                _, offset = read_unsigned(data, offset)
            return names
        offset = end
    return set()


output = Path(sys.argv[1]).resolve()
files = [output / name for name in ("quickjs.wasm", "opus_frame_decoder.wasm", "webp_encoder.wasm")]
for path in files:
    if path.read_bytes()[:8] != b"\x00asm\x01\x00\x00\x00":
        raise ValueError("Invalid core Wasm module " + str(path))
sources = ("quickjs/quickjs_bridge.c", "opus_decoder/opus_decoder.c", "webp_encoder/webp_encoder.c")
root = Path(__file__).resolve().parents[1]
for path, source in zip(files, sources):
    expected = set(re.findall(r'export_name\("([^"]+)"\)', (root / source).read_text()))
    expected.add("memory")
    missing = expected - exports(path.read_bytes())
    if missing:
        raise ValueError(f"Missing exports in {path.name}: {sorted(missing)}")
manifest = {
    "commit": subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip(),
    "dirty": bool(subprocess.check_output(["git", "-C", str(root), "status", "--porcelain"])),
    "toolchain": {
        "zig": subprocess.check_output(["zig", "version"], text=True).strip(),
        "cmake": subprocess.check_output(["cmake", "--version"], text=True).splitlines()[0],
        "binaryen": subprocess.check_output(["wasm-opt", "--version"], text=True).strip()
        if shutil.which("wasm-opt") else None,
    },
    "sources": json.loads((root / "tool/sources.json").read_text()),
    "modules": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files},
}
(output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
archive = output / "oronbox_wasm_modules.zip"
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zipped:
    for path in sorted(output.rglob("*")):
        if path.is_file() and path.suffix not in (".zip", ".sha256"):
            zipped.write(path, path.relative_to(output))
(output / "oronbox_wasm_modules.zip.sha256").write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n")
print(archive)
