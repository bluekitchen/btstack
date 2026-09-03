#!/usr/bin/env python3
"""Update src/btstack_version.h from the latest CHANGELOG.md release."""

import re
import subprocess
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
CHANGELOG_PATH = REPOSITORY_ROOT / "CHANGELOG.md"
VERSION_HEADER_PATH = REPOSITORY_ROOT / "src" / "btstack_version.h"


def latest_release_version():
    changelog = CHANGELOG_PATH.read_text(encoding="utf-8")
    match = re.search(r"^## Release v(\d+\.\d+\.\d+)[ \t]*$", changelog, re.MULTILINE)
    if match is None:
        raise ValueError("no release tag found in CHANGELOG.md")
    return match.group(1)


def update_header(version):
    header = VERSION_HEADER_PATH.read_text(encoding="utf-8")
    components = dict(zip(("MAJOR", "MINOR", "PATCH"), version.split(".")))

    for name, value in components.items():
        pattern = re.compile(
            rf"^(#define BTSTACK_VERSION_{name}[ \t]+)\d+([ \t]*)$", re.MULTILINE
        )
        header, replacements = pattern.subn(
            lambda match: f"{match.group(1)}{value}{match.group(2)}", header, count=1
        )
        if replacements != 1:
            raise ValueError(f"BTSTACK_VERSION_{name} is missing or invalid")

    previous_header = VERSION_HEADER_PATH.read_text(encoding="utf-8")
    if header == previous_header:
        return False

    VERSION_HEADER_PATH.write_text(header, encoding="utf-8")
    return True


def commit_version_update(version):
    subprocess.run(
        [
            "git",
            "commit",
            "--only",
            "-m",
            f"btstack_version.h: bump version to {version}",
            "--",
            "src/btstack_version.h",
        ],
        cwd=REPOSITORY_ROOT,
        check=True,
    )


def main():
    try:
        version = latest_release_version()
        updated = update_header(version)
    except (OSError, ValueError) as error:
        print(f"version update error: {error}", file=sys.stderr)
        return 1

    if not updated:
        print(f"src/btstack_version.h already contains version {version}")
        return 0

    print(f"updated src/btstack_version.h to version {version}")
    try:
        commit_version_update(version)
    except subprocess.CalledProcessError:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
