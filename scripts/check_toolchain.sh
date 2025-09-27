#!/usr/bin/env bash
set -euo pipefail

F256DEV_ROOT=${F256DEV_ROOT:-/home/john/code/llvm-mos/f256dev}
MOS_BIN=${MOS_BIN:-"${F256DEV_ROOT}/llvm-mos/bin"}

MOS_CC=${MOS_CC:-"${MOS_BIN}/mos-f256-clang"}
MOS_AR=${MOS_AR:-"${MOS_BIN}/llvm-ar"}
MOS_OBJCOPY=${MOS_OBJCOPY:-"${MOS_BIN}/llvm-objcopy"}

echo "Verifying Foenix F256 llvm-mos toolchain availability..."

for tool in "$MOS_CC" "$MOS_AR" "$MOS_OBJCOPY"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing tool: $tool" >&2
        exit 1
    fi
    "$tool" --version | head -n 1 || echo "(version info unavailable)"
done

CONFIG=${MOS_CONFIG:-"${MOS_BIN}/mos-f256.cfg"}
if [[ ! -f "$CONFIG" ]]; then
    echo "Missing compiler config file: $CONFIG" >&2
    exit 1
fi

echo "Detected config: $CONFIG"
echo "Toolchain check complete."