import os
import re
import subprocess
from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
version = os.environ.get("APP_VERSION_OVERRIDE", "").strip()
if not version:
    version = (project_dir / "VERSION").read_text(encoding="ascii").strip()
version_match = re.fullmatch(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", version)
if not version_match or any(int(part) > 9999 for part in version_match.groups()):
    raise ValueError(f"invalid firmware version: {version!r}")

build = os.environ.get("GITHUB_SHA", "").strip()[:7]
if not build:
    try:
        build = subprocess.check_output(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=project_dir,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        build = "local"

env.Append(
    CPPDEFINES=[
        ("APP_VERSION", env.StringifyMacro(version)),
        ("APP_BUILD", env.StringifyMacro(build)),
    ]
)
