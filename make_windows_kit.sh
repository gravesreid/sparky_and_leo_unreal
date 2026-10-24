#!/usr/bin/env bash
# Bundles what a Windows PC needs to build the Windows version (see package_windows.bat):
# the code, content and config, but none of this machine's build output. Writes SparkyLeo-WindowsKit.zip.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/SparkyLeo"
cp -r "$HERE/Source" "$HERE/Content" "$HERE/Config" "$HERE/package_windows.bat" "$HERE/HOW_TO_PLAY.txt" "$HERE/README.md" "$STAGE/SparkyLeo/"
rm -rf "$STAGE/SparkyLeo/Content/Developers" "$STAGE/SparkyLeo/Content/Collections"
# this machine's engine is a source build with its own ID; the launcher's engine is just "5.2"
sed 's/"EngineAssociation": "[^"]*"/"EngineAssociation": "5.2"/' "$HERE/SparkyLeo.uproject" > "$STAGE/SparkyLeo/SparkyLeo.uproject"
# Windows wants CRLF line endings in batch files
sed -i 's/\r*$/\r/' "$STAGE/SparkyLeo/package_windows.bat" "$STAGE/SparkyLeo/HOW_TO_PLAY.txt"
rm -f "$HERE/SparkyLeo-WindowsKit.zip"
(cd "$STAGE" && zip -qr "$HERE/SparkyLeo-WindowsKit.zip" SparkyLeo)
echo "Wrote $HERE/SparkyLeo-WindowsKit.zip ($(du -h "$HERE/SparkyLeo-WindowsKit.zip" | cut -f1))"
