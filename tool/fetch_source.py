#!/usr/bin/env python3
"""Download and verify a pinned upstream source archive before extraction."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def fetch_source(module):
    source = json.loads((ROOT / "tool/sources.json").read_text())[module]
    cache = ROOT / module / ".build"
    cache.mkdir(parents=True, exist_ok=True)
    directory = cache / source["directory"]
    archive = cache / (source["directory"] + ".tar.gz")
    stamp = cache / (source["directory"] + ".sha256")
    with tempfile.TemporaryDirectory(dir=cache) as temporary:
        temporary = Path(temporary)
        downloaded = archive if archive.exists() else temporary / "source.tar.gz"
        if not downloaded.exists():
            subprocess.run([
                "curl", "-fLsS", "--retry", "3", "--output", str(downloaded), source["url"],
            ], check=True)
        if hashlib.sha256(downloaded.read_bytes()).hexdigest() != source["sha256"]:
            downloaded.unlink()
            raise ValueError(f"Source SHA-256 mismatch for {module}")
        if downloaded != archive:
            downloaded.replace(archive)
        if directory.is_dir() and stamp.exists() and stamp.read_text().strip() == source["sha256"]:
            return directory
        with tarfile.open(archive) as compressed:
            compressed.extractall(temporary, filter="data")
        extracted = temporary / source["directory"]
        if not extracted.is_dir():
            raise ValueError(f"Source archive directory missing for {module}")
        if directory.exists():
            shutil.rmtree(directory)
        extracted.replace(directory)
        stamp.write_text(source["sha256"] + "\n")
    return directory


if __name__ == "__main__":
    print(fetch_source(sys.argv[1]))
