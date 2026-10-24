#!/usr/bin/env bash
# Plays Sparky and Leo from wherever this folder is (e.g. a USB stick). Run with:  bash Play_SparkyLeo.sh
# Extra args go to the game, e.g.  bash Play_SparkyLeo.sh -Chapter=3
# If the drive is mounted without exec permission, the game is copied to ~/.cache/SparkyLeo and run from there.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/SparkyLeo/Binaries/Linux/SparkyLeo-Linux-Shipping"
if findmnt -no OPTIONS --target "$HERE" | grep -qw noexec; then
    echo "Drive is noexec; copying the game to ~/.cache/SparkyLeo ..."
    mkdir -p "$HOME/.cache/SparkyLeo"
    cp -ru "$HERE/SparkyLeo" "$HOME/.cache/SparkyLeo/"
    chmod +x "$HOME/.cache/SparkyLeo/SparkyLeo/Binaries/Linux/"*
    BIN="$HOME/.cache/SparkyLeo/SparkyLeo/Binaries/Linux/SparkyLeo-Linux-Shipping"
fi
exec "$BIN" SparkyLeo -windowed -ResX=1280 -ResY=960 "$@"
