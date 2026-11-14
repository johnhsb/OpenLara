#!/bin/bash
# Builds the WebAssembly version into out/ with Emscripten (em++ in the PATH, e.g. after "source ~/emsdk/emsdk_env.sh"),
# for the levels of the PC versions served next to the page:
#   ./build.sh [TR1 folder] [TR2 folder]   links the game data into out/ (level/1, audio/1, level/2, audio/2)
#   cd out && python3 -m http.server 8000  then open http://localhost:8000/ (?level=level/2/WALL.TR2 starts a level)
set -e
cd "$(dirname "$0")"
mkdir -p out

em++ main.cpp ../../libs/stb_vorbis/stb_vorbis.c ../../libs/tinf/tinflate.c \
    -O3 -ffast-math -std=c++11 -DNDEBUG -DWEB_PC_DATA -I../../ \
    -Wno-invalid-source-encoding -Wno-deprecated-register \
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAX_WEBGL_VERSION=2 \
    -sEXPORTED_FUNCTIONS=_main,_malloc,_free \
    -sEXPORTED_RUNTIME_METHODS=ccall,getValue,writeArrayToMemory,stringToNewUTF8 \
    $EM_EXTRA -o out/OpenLara.js

cp index.html out/

# the data of a game: its levels by their upper case names, its music and sounds
link() { # <game folder> <1|2>
    local src="$1" n="$2" f
    mkdir -p "out/level/$n" "out/audio/$n"
    for f in "$src"/[Dd][Aa][Tt][Aa]/*; do
        [ -f "$f" ] || continue
        case "$f" in *:Zone.Identifier) continue;; esac
        local name="$(basename "$f" | tr a-z A-Z)"
        if [ "$name" = "MAIN.SFX" ]; then ln -sf "$f" "out/audio/$n/$name"; else ln -sf "$f" "out/level/$n/$name"; fi
    done
    for f in "$src"/audio/*.ogg; do
        [ -f "$f" ] && ln -sf "$f" "out/audio/$n/$(basename "$f")"
    done
    for f in "$src"/music/[Tt]rack*.ogg; do # the music pack of the PC version of TR2: TrackNN.ogg
        [ -f "$f" ] || continue
        local num="$(basename "$f" .ogg | tr -dc 0-9)"
        ln -sf "$f" "out/audio/$n/track_$num.ogg"
    done
}

[ -n "$1" ] && link "$1" 1
[ -n "$2" ] && link "$2" 2
echo "built out/OpenLara.js, out/OpenLara.wasm"
