#!/bin/sh
# Update and commit src/btstack_version.h for the latest CHANGELOG.md release.

set -eu

SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd)
exec python3 "$SCRIPT_DIR/update_btstack_version.py"
