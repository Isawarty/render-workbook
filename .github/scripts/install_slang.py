#!/usr/bin/env python3
"""Install the pinned Slang compiler on GitHub Actions runners."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import zipfile


VERSION = "2026.14"
RELEASE_ROOT = f"https://github.com/shader-slang/slang/releases/download/v{VERSION}"

# Official release assets and SHA-256 digests from shader-slang/slang v2026.14.
ASSETS = {
    ("Linux", "x86_64"): (
        f"slang-{VERSION}-linux-x86_64-glibc-2.27.tar.gz",
        "b18292abd709e56eae1f26a52c5431a97e9f2225891d937064cb68edb92e19ca",
    ),
    ("Darwin", "arm64"): (
        "slang-macos-dist-aarch64.zip",
        "e6552555f37fdf5efbe08665e91579082fcc5c5945276a8d695ea879edc685f2",
    ),
    ("Windows", "AMD64"): (
        f"slang-{VERSION}-windows-x86_64.zip",
        "36029c50ef0c82f2616ffb02e0ed27d642cb44a2a297d531cc2ad333b85b85b6",
    ),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    key = (platform.system(), platform.machine())
    if key not in ASSETS:
        supported = ", ".join(f"{system}/{arch}" for system, arch in ASSETS)
        raise SystemExit(f"Unsupported Slang CI host {key[0]}/{key[1]}; expected {supported}")

    runner_temp = Path(os.environ.get("RUNNER_TEMP", ".ci-tools")).resolve()
    install_dir = runner_temp / f"slang-{VERSION}"
    archive_name, expected_digest = ASSETS[key]
    archive = runner_temp / archive_name

    shutil.rmtree(install_dir, ignore_errors=True)
    install_dir.mkdir(parents=True)
    print(f"Downloading Slang {VERSION}: {archive_name}", flush=True)
    urllib.request.urlretrieve(f"{RELEASE_ROOT}/{archive_name}", archive)

    actual_digest = sha256(archive)
    if actual_digest != expected_digest:
        raise SystemExit(
            f"Slang archive checksum mismatch: expected {expected_digest}, got {actual_digest}"
        )

    if archive_name.endswith(".zip"):
        with zipfile.ZipFile(archive) as package:
            package.extractall(install_dir)
    else:
        with tarfile.open(archive, "r:gz") as package:
            package.extractall(install_dir, filter="data")

    executable_name = "slangc.exe" if key[0] == "Windows" else "slangc"
    matches = list(install_dir.rglob(executable_name))
    if len(matches) != 1:
        raise SystemExit(f"Expected one {executable_name} in {install_dir}, found {len(matches)}")

    slangc = matches[0]
    if key[0] != "Windows":
        slangc.chmod(slangc.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    github_path = os.environ.get("GITHUB_PATH")
    if github_path:
        with Path(github_path).open("a", encoding="utf-8") as stream:
            stream.write(f"{slangc.parent}{os.linesep}")
    else:
        print(f"Add this directory to PATH: {slangc.parent}")

    subprocess.run([str(slangc), "-version"], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
