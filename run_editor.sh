#!/usr/bin/env bash
# Opens the project in the Unreal editor (press Play, or Alt+P).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
UE="${UE_ROOT:-$HOME/unreal_engine/Linux_Unreal_Engine_5.2.0}"
exec "$UE/Engine/Binaries/Linux/UnrealEditor" "$HERE/SparkyLeo.uproject" "$@"
