#!/bin/bash
# Save source edits, including new files and the mobile filesystem watcher shim.
set -eu
cd "$(dirname "$0")/.."
exec python3 scripts/export-patches.py
