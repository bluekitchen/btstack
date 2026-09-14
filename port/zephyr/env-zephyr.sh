#!/bin/echo "Error: This script must be sourced, not executed"

# Source this file before running west from port/zephyr with a plain upstream
# Zephyr workspace.

BTSTACK_ZEPHYR_ROOT="${BTSTACK_ZEPHYR_ROOT:-$HOME/zephyrproject}"
ZEPHYR_BASE="${BTSTACK_ZEPHYR_BASE:-$BTSTACK_ZEPHYR_ROOT/zephyr}"

if [ -f "$BTSTACK_ZEPHYR_ROOT/.venv/bin/activate" ]; then
    # shellcheck disable=SC1091
    . "$BTSTACK_ZEPHYR_ROOT/.venv/bin/activate"
fi

if [ ! -f "$ZEPHYR_BASE/zephyr-env.sh" ]; then
    echo "BTstack Zephyr: cannot find $ZEPHYR_BASE/zephyr-env.sh" >&2
    echo "Set BTSTACK_ZEPHYR_ROOT or BTSTACK_ZEPHYR_BASE before sourcing env-zephyr.sh." >&2
    return 1
fi

export BTSTACK_ZEPHYR_ROOT
export ZEPHYR_BASE

# shellcheck disable=SC1091
. "$ZEPHYR_BASE/zephyr-env.sh"
