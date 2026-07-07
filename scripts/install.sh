#!/usr/bin/env bash
# install.sh — deploy Groove Bank module to Ableton Move via SSH
#
# Usage:
#   ./scripts/install.sh            # deploy to move.local
#   ./scripts/install.sh 192.168.x.x
#
# Prerequisites:
#   - Move on USB-C (move.local) or same WiFi, SSH enabled
#   - dsp.so built:  ./scripts/build.sh

set -euo pipefail
cd "$(dirname "$0")/.."

MOVE_HOST="${1:-move.local}"
MOVE_USER="root"
MOVE_MODULES_DIR="/data/UserData/schwung/modules"
MODULE_CATEGORY="midi_fx"
MODULE_ID="groovebank"
MODDIR="${MOVE_MODULES_DIR}/${MODULE_CATEGORY}/${MODULE_ID}"
DEST="${MOVE_USER}@${MOVE_HOST}:${MODDIR}"

DSO="build/aarch64/dsp.so"

if [ ! -f "$DSO" ]; then
    echo "✗ $DSO not found. Run ./scripts/build.sh first."
    exit 1
fi

echo "→ Deploying to ${MOVE_HOST} …"
ssh "${MOVE_USER}@${MOVE_HOST}" "mkdir -p ${MODDIR}/patterns"

# dsp.so may be mmap'd by the running chain — overwriting it in place can hang.
# scp to a temp name, then atomically rename over the old inode.
scp "$DSO"                  "${DEST}/dsp.so.new"
ssh "${MOVE_USER}@${MOVE_HOST}" "mv -f ${MODDIR}/dsp.so.new ${MODDIR}/dsp.so"

scp src/module.json        "${DEST}/module.json"
scp src/canvas.js          "${DEST}/canvas.js"
scp src/help.json          "${DEST}/help.json"
scp src/patterns/*.groove  "${DEST}/patterns/"

echo ""
echo "✓ Installed to ${MOVE_MODULES_DIR}/${MODULE_CATEGORY}/${MODULE_ID}/"
echo ""
echo "  On Move: add Groove Bank as the MIDI FX of a chain slot whose"
echo "  Sound Generator is a synth, hold a chord, and press Play."
