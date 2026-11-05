#!/bin/bash
# Builds an upscaled texture pack for the levels of a game folder.
#
#   texture_pack.sh <game folder> [upscaler] [scale] [model] [format]
#
# 1. writes the original texture tiles of every level to <game folder>/texture/original/ (OL_DUMP_TEXTURES=1, a game
#    window shows up for a moment for each level)
# 2. with an upscaler given, upscales them with Real-ESRGAN (realesrgan-ncnn-vulkan, the .exe of Windows works from WSL
#    on the GPU of Windows) into <game folder>/texture/, where OpenLara takes them in place of the original tiles
#
# scale:  4 (default) or 2 (the x4 result scaled down, a quarter of the texture memory)
# model:  realesrgan-x4plus (default) or realesrgan-x4plus-anime
# format: jpg (default, about a tenth of the size of PNG, the cut-outs come from the original tiles) or png
set -e

if [ -z "$1" ]; then
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
fi

GAME=$(cd "$1" && pwd)
ESRGAN=$2
SCALE=${3:-4}
MODEL=${4:-realesrgan-x4plus}
FORMAT=${5:-jpg}
BIN=$(cd "$(dirname "$0")" && pwd)/OpenLara

cd "$GAME"

for level in data/*.[tT][rR]2 data/*.[pP][hH][dD] data/*.[tT][rR]4; do
    [ -f "$level" ] || continue
    echo "export $level"
    OL_DUMP_TEXTURES=1 "$BIN" -l "$level" > /dev/null
done

if [ -z "$ESRGAN" ]; then
    echo "the original tiles are in $GAME/texture/original"
    exit 0
fi

case "$ESRGAN" in
    *.exe) osPath() { wslpath -w "$1"; } ;;
    *)     osPath() { echo "$1"; } ;;
esac

echo "upscale x4 with $MODEL"
"$ESRGAN" -i "$(osPath "$GAME/texture/original")" -o "$(osPath "$GAME/texture")" -n "$MODEL" -s 4 -f png

if ! ls texture/*.png > /dev/null 2>&1; then # the upscaler writes PNG
    echo "the upscaler made no tiles in $GAME/texture, see its messages above"
    exit 1
fi

SIZE=$((256 * SCALE))
if [ "$SCALE" != "4" ] || [ "$FORMAT" = "jpg" ]; then
    echo "to x$SCALE $FORMAT"
    for f in texture/*.png; do
        if [ "$FORMAT" = "jpg" ]; then
            ffmpeg -loglevel error -y -i "$f" -vf scale=$SIZE:$SIZE:flags=lanczos -q:v 3 "${f%.png}.jpg"
            rm "$f"
        else
            ffmpeg -loglevel error -y -i "$f" -vf scale=$SIZE:$SIZE:flags=lanczos -pix_fmt rgba "$f.tmp.png"
            mv "$f.tmp.png" "$f"
        fi
    done
fi

echo "the texture pack is in $GAME/texture"
