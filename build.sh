#!/usr/bin/env bash
# Compiles the C++ module (run after editing Source/, with the editor closed).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
UE="${UE_ROOT:-$HOME/unreal_engine/Linux_Unreal_Engine_5.2.0}"
exec "$UE/Engine/Build/BatchFiles/Linux/Build.sh" SparkyLeoEditor Linux Development -Project="$HERE/SparkyLeo.uproject" -WaitMutex
