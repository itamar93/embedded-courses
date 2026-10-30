#!/usr/bin/env bash
# Builds qemu-system-arm with the blackpill-f411 machine.
# Usage: emulator/qemu/build.sh [install-prefix]   (default: ./emulator/qemu/out)
set -euo pipefail

QEMU_VERSION=v10.2.4
HERE="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${1:-$HERE/out}"
SRC="$HERE/src"

if [ ! -d "$SRC" ]; then
    git clone --depth 1 --branch "$QEMU_VERSION" https://gitlab.com/qemu-project/qemu.git "$SRC"
    git -C "$SRC" apply "$HERE/0001-blackpill-board.patch"
    cp "$HERE/blackpill.c" "$SRC/hw/arm/blackpill.c"
    cp "$HERE/blackpill.mak" "$SRC/configs/devices/arm-softmmu/blackpill.mak"
fi

mkdir -p "$SRC/build"
cd "$SRC/build"
../configure --prefix="$PREFIX" --target-list=arm-softmmu \
    --with-devices-arm=blackpill --without-default-devices \
    --disable-docs --disable-tools --disable-gtk --disable-sdl --disable-vnc \
    --disable-opengl --disable-slirp --disable-werror
ninja qemu-system-arm
mkdir -p "$PREFIX/bin"
cp qemu-system-arm "$PREFIX/bin/"
echo "Built $PREFIX/bin/qemu-system-arm"
