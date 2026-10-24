#!/usr/bin/env bash
# Plays the game (uncooked, from the editor build), in a 4:3 window. Extra args go to the engine, e.g.
#   ./run_game.sh -Chapter=3      start at chapter 3
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
UE="${UE_ROOT:-$HOME/unreal_engine/Linux_Unreal_Engine_5.2.0}"
exec "$UE/Engine/Binaries/Linux/UnrealEditor" "$HERE/SparkyLeo.uproject" -game -windowed -ResX=1280 -ResY=960 -log "$@"
