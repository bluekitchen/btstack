#!/usr/bin/env python3

"""Verify that the source version matches the latest changelog release."""

import re
import sys
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
CHANGELOG_PATH = REPOSITORY_ROOT / "CHANGELOG.md"
VERSION_HEADER_PATH = REPOSITORY_ROOT / "src" / "btstack_version.h"


def version_from_changelog():
    match = re.search(
        r"^## Release v(\d+\.\d+\.\d+)\s*$",
        CHANGELOG_PATH.read_text(encoding="utf-8"),
        re.MULTILINE,
    )
    if match is None:
        raise ValueError("no release tag found")
    return match.group(1)


def version_from_header():
    header = VERSION_HEADER_PATH.read_text(encoding="utf-8")
    components = []
    for name in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rf"^#define BTSTACK_VERSION_{name}\s+(\d+)\s*$", header, re.MULTILINE)
        if match is None:
            raise ValueError(f"BTSTACK_VERSION_{name} is missing or invalid")
        components.append(match.group(1))
    return ".".join(components)


def main():
    try:
        changelog_version = version_from_changelog()
        header_version = version_from_header()
    except (OSError, ValueError) as error:
        print(f"version test error: {error}", file=sys.stderr)
        return 1

    if header_version != changelog_version:
        print(
            f"version mismatch: src/btstack_version.h is {header_version}, "
            f"but CHANGELOG.md latest release is {changelog_version}",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
