#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import re
from pathlib import Path

VERSION_PATTERN = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
MAX_FIRMWARE_SIZE = 0x140000
PROJECT = "smart-abajour"
TARGET = "mhetesp32minikit"
REPOSITORY = "brunoricardojava/smart-abajour"
ASSET_NAME = "firmware-mhetesp32minikit.bin"


def read_version(path: Path) -> str:
    version = path.read_text(encoding="ascii").strip()
    match = VERSION_PATTERN.fullmatch(version)
    if not match or any(int(part) > 9999 for part in match.groups()):
        raise ValueError(f"invalid semantic version: {version!r}")
    return version


def build_manifest(version: str, tag: str, build: str, firmware: Path) -> dict:
    if tag != f"v{version}":
        raise ValueError(f"tag {tag!r} does not match VERSION {version!r}")
    if not re.fullmatch(r"[0-9a-f]{7,40}", build):
        raise ValueError(f"invalid Git commit: {build!r}")

    size = firmware.stat().st_size
    if size <= 0 or size > MAX_FIRMWARE_SIZE:
        raise ValueError(
            f"firmware size {size} exceeds OTA slot limit {MAX_FIRMWARE_SIZE}"
        )

    digest = hashlib.sha256(firmware.read_bytes()).hexdigest()
    return {
        "schema": 1,
        "project": PROJECT,
        "target": TARGET,
        "version": version,
        "build": build[:7],
        "size": size,
        "sha256": digest,
        "url": f"https://github.com/{REPOSITORY}/releases/download/{tag}/{ASSET_NAME}",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the ESP32 OTA manifest")
    parser.add_argument("--version-file", type=Path, default=Path("VERSION"))
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--build", default=os.environ.get("GITHUB_SHA", ""))
    args = parser.parse_args()

    version = read_version(args.version_file)
    manifest = build_manifest(version, args.tag, args.build, args.firmware)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=True) + "\n",
        encoding="ascii",
    )


if __name__ == "__main__":
    main()
