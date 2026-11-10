#!/usr/bin/env python3
"""Print the effective ENABLE_* and HAVE_* macros from a BTstack configuration header.

The C preprocessor, rather than this script, evaluates conditional definitions
and included headers.  The output is one macro name per line, sorted and
de-duplicated, so it can be consumed by CMake or inspected directly.
"""

import argparse
import re
import subprocess
import sys


CONFIG_DEFINE = re.compile(r"^#define\s+((?:ENABLE|HAVE)_[A-Z0-9_]+)(?:\s|$)")
DEFINE = re.compile(r"^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)")


def extract_msvc_config(config: str, definitions: list[str]) -> set[str]:
    """Extract simple configuration-header definitions when using MSVC.

    MSVC has no equivalent of GCC's ``-dM`` option.  BTstack configuration
    headers deliberately use only straightforward #ifdef guards, so retain a
    small conditional parser for that compiler instead of requiring Clang just
    to configure a native Visual Studio build.
    """
    defined = {definition.split("=", 1)[0] for definition in definitions}
    active = [True]
    macros = set()
    for raw_line in open(config, encoding="utf-8"):
        line = raw_line.strip()
        if line.startswith("#ifdef "):
            active.append(active[-1] and line[7:].strip() in defined)
            continue
        if line.startswith("#ifndef "):
            active.append(active[-1] and line[8:].strip() not in defined)
            continue
        if line.startswith("#else") and len(active) > 1:
            parent = active[-2]
            active[-1] = parent and not active[-1]
            continue
        if line.startswith("#endif") and len(active) > 1:
            active.pop()
            continue
        match = DEFINE.match(line)
        if match and active[-1]:
            name = match.group(1)
            defined.add(name)
            if name.startswith(("ENABLE_", "HAVE_")):
                macros.add(name)
    return macros


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True,
                        help="C compiler used to run the preprocessor")
    parser.add_argument("--config", required=True,
                        help="path to btstack_config.h")
    parser.add_argument("--define", action="append", default=[],
                        help="preprocessor definition required by the configuration (NAME or NAME=VALUE)")
    parser.add_argument("--include-directory", action="append", default=[],
                        help="include directory required by the configuration")
    args = parser.parse_args()

    compiler_name = args.compiler.replace("\\", "/").rsplit("/", 1)[-1].lower()
    if compiler_name in {"cl", "cl.exe"}:
        for macro in sorted(extract_msvc_config(args.config, args.define)):
            print(macro)
        return 0

    command = [args.compiler]
    command.extend(f"-D{definition}" for definition in args.define)
    command.extend(f"-I{directory}" for directory in args.include_directory)
    command.extend(["-dM", "-E", "-x", "c", "-include", args.config, "-"])

    result = subprocess.run(command, input="", text=True, capture_output=True)
    if result.returncode:
        print("Failed to preprocess BTstack configuration:", file=sys.stderr)
        print(" ".join(command), file=sys.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        return result.returncode

    config_macros = set()
    for line in result.stdout.splitlines():
        match = CONFIG_DEFINE.match(line)
        if match:
            config_macros.add(match.group(1))

    for macro in sorted(config_macros):
        print(macro)
    return 0


if __name__ == "__main__":
    sys.exit(main())
