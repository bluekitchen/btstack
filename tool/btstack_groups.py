#!/usr/bin/env python3
"""List BTstack CMake source groups and inspect their requirements."""

import argparse
from pathlib import Path
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    list_parser = commands.add_parser("list", help="list available source groups")
    list_parser.add_argument("--filter", default="", help="case-insensitive substring to match")
    info_parser = commands.add_parser("info", help="show details for a source group")
    info_parser.add_argument("group", help="source group name, e.g. gatt-server")
    args = parser.parse_args()

    script = Path(__file__).resolve().parent / "cmake" / "BTstackGroups.cmake"
    command = ["cmake", f"-DMODE={args.command}"]
    if args.command == "list":
        command.append(f"-DFILTER={args.filter}")
    else:
        command.append(f"-DGROUP={args.group}")
    command.extend(["-P", str(script)])
    try:
        return subprocess.run(command, check=False).returncode
    except FileNotFoundError:
        print("Error: CMake was not found; install it and ensure it is on PATH.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
