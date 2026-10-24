#!/usr/bin/env bash
# Packages a standalone Linux Shipping build into Packaged/Linux, with the Play_SparkyLeo.sh launcher.
# Copy Packaged/Linux/{SparkyLeo,Play_SparkyLeo.sh} anywhere (e.g. a USB stick) and run: bash Play_SparkyLeo.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
UE="${UE_ROOT:-$HOME/unreal_engine/Linux_Unreal_Engine_5.2.0}"
# ~/.local/bin/env (left by the uv installer) shadows /usr/bin/env and breaks the staging step
PATH="$(echo "$PATH" | tr ':' '\n' | grep -vx "$HOME/.local/bin" | paste -sd:)"
"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$HERE/SparkyLeo.uproject" -platform=Linux \
    -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -package -archive \
    -archivedirectory="$HERE/Packaged" -nodebuginfo -unattended -utf8output
cp "$HERE/Scripts/Play_SparkyLeo.sh" "$HERE/Packaged/Linux/"
echo "Packaged to $HERE/Packaged/Linux"
