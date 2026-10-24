#!/usr/bin/env bash
# Synthesizes the audio and textures (Scripts/make_audio.py, Scripts/make_textures.py), then
# imports them and creates missing materials / the map (Scripts/build_content.py, headless editor).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
UE="${UE_ROOT:-$HOME/unreal_engine/Linux_Unreal_Engine_5.2.0}"
python3 "$HERE/Scripts/make_textures.py"
python3 "$HERE/Scripts/make_audio.py"
exec "$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$HERE/SparkyLeo.uproject" \
    -run=pythonscript -script="$HERE/Scripts/build_content.py" -unattended -nosplash -nopause -stdout -FullStdOutLogOutput
