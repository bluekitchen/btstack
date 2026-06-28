#!/bin/echo "Error: This script must be sourced, not executed"

# Source this file before running west from port/zephyr with Nordic's nRF
# Connect SDK managed by nrfutil sdk-manager.

BTSTACK_NRFUTIL="${BTSTACK_NRFUTIL:-nrfutil}"

btstack_find_ncs_root_and_version() {
    btstack_ncs_version="$1"
    btstack_nrfutil="${BTSTACK_NRFUTIL:-nrfutil}"

    "$btstack_nrfutil" sdk-manager list --json --skip-overhead 2>/dev/null | python3 -c '
import json
import re
import sys

requested = sys.argv[1]
raw = sys.stdin.read()
if not raw.strip():
    sys.exit(1)

data = json.loads(raw)
installed = []

for entry in data.get("versions", []):
    if entry.get("type") != "nrf":
        continue
    if entry.get("sdkStatus") != "installed":
        continue
    dir_names = entry.get("dirNames") or []
    if not dir_names:
        continue
    version = entry.get("version")
    if requested and version != requested:
        continue
    installed.append((version, dir_names[0]))

if not installed:
    sys.exit(1)

def version_key(item):
    version = item[0]
    match = re.match(r"^v?(\d+)\.(\d+)\.(\d+)(.*)$", version or "")
    if not match:
        return (-1, -1, -1, version or "")
    return (int(match.group(1)), int(match.group(2)), int(match.group(3)), match.group(4))

version, root = sorted(installed, key=version_key)[-1]
print(f"{version}\n{root}")
' "${btstack_ncs_version:-}"
}

if ! command -v "$BTSTACK_NRFUTIL" >/dev/null 2>&1; then
    echo "BTstack Zephyr: cannot find nrfutil. Set BTSTACK_NRFUTIL if it is not in PATH." >&2
    return 1
fi

if [ -z "${BTSTACK_NCS_ROOT:-}" ]; then
    btstack_ncs_info="$(btstack_find_ncs_root_and_version "${BTSTACK_NCS_VERSION:-}")"
    if [ -n "$btstack_ncs_info" ]; then
        BTSTACK_NCS_VERSION="$(printf "%s\n" "$btstack_ncs_info" | sed -n '1p')"
        BTSTACK_NCS_ROOT="$(printf "%s\n" "$btstack_ncs_info" | sed -n '2p')"
    fi
elif [ -z "${BTSTACK_NCS_VERSION:-}" ]; then
    BTSTACK_NCS_VERSION="$(basename "$BTSTACK_NCS_ROOT")"
fi

if [ -z "${BTSTACK_NCS_ROOT:-}" ] || [ ! -d "$BTSTACK_NCS_ROOT/zephyr" ]; then
    echo "BTstack Zephyr: cannot find an installed NCS workspace." >&2
    echo "Install one with nrfutil sdk-manager, set BTSTACK_NCS_VERSION, or set BTSTACK_NCS_ROOT." >&2
    return 1
fi

btstack_ncs_env="$("$BTSTACK_NRFUTIL" sdk-manager toolchain env --ncs-version "$BTSTACK_NCS_VERSION" --as-script sh)" || {
    echo "BTstack Zephyr: failed to activate NCS toolchain for $BTSTACK_NCS_VERSION." >&2
    return 1
}
eval "$btstack_ncs_env"

ZEPHYR_BASE="$BTSTACK_NCS_ROOT/zephyr"
export BTSTACK_NRFUTIL
export BTSTACK_NCS_ROOT
export BTSTACK_NCS_VERSION
export ZEPHYR_BASE

echo "Using NCS $BTSTACK_NCS_VERSION"
echo "ZEPHYR_BASE=$ZEPHYR_BASE"

unset btstack_ncs_info
unset btstack_ncs_env
