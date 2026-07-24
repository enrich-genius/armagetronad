#!/bin/bash
# Emscripten build for Armagetron Advanced (release_0.2.9) -> WebAssembly.
#
# Compiles the client sources with emcc, mapping SDL/OpenGL to the browser's
# WebGL + Web Audio via emscripten's SDL1.2 ports, and packages the game data.
#
# Layout is discovered relative to this script; override with env vars:
#   EMSDK_DIR      path to an emsdk checkout   (default: <workspace>/emsdk)
#   WASM_DEPS_DIR  prefix with wasm libxml2    (default: <workspace>/wasm-deps)
# where <workspace> is the directory two levels above this script.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$HERE/../src" && pwd)"
WORKSPACE="$(cd "$HERE/../.." && pwd)"

EMSDK_DIR="${EMSDK_DIR:-$WORKSPACE/emsdk}"
WASM_DEPS_DIR="${WASM_DEPS_DIR:-$WORKSPACE/wasm-deps}"
OUT="$HERE/dist"
mkdir -p "$OUT"

# Activate emscripten (respect an already-sourced environment if emcc is present).
if ! command -v emcc >/dev/null 2>&1; then
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
fi

# -- source file list (mirrors src/Makefile.am, client/BUILDMAIN subset) -------
TOOLS="tArray tCallback tColor tConfiguration tConsole tCrypt tDirectories \
  tError tEventQueue tHeap tLinkedList tLocale tMemStack tRing tSafePTR \
  tString tSysTime tToDo tException tRecorder tRecorderInternal tCommandLine \
  tRandom tMemManager tResourceManager"
NETWORK="md5 nAuthentication nConfig nKrawall nKrawallPrivate nNetObject \
  nNetwork nObserver nPriorizing nServerInfo nSocket nSpamProtection"
ENGINE="eAdvWall eAuthentication eAxis eCamera eDebugLine eDisplay eFloor \
  eGameObject eGrid eKrawall eNetGameObject ePath ePlayer eRectangle eSensor \
  eSound eTeam eTimer eVoter eWall eLagCompensation eChat"
RENDER="rConsole rConsoleGraph rFont rGL rGLRender rModel rRender rDisplayList \
  rScreen rSysdep rTexture rViewport"
TRON="gAIBase gAICharacter gArena gArmagetron gCamera gCycle gCycleMovement \
  gExplosion gFloor gGame gHud gLanguageMenu gLogo gMenus gParser gParticles \
  gSensor gServerBrowser gSparks gSpawn gStuff gTeam gWall gWinZone gFriends \
  gServerFavorites"
UI="uInput uInputQueue uMenu"
PARTICLES="action_api actions opengl system"

SOURCES=()
for f in $TOOLS;    do SOURCES+=("$SRC/tools/$f.cpp"); done
for f in $NETWORK;  do SOURCES+=("$SRC/network/$f.cpp"); done
for f in $ENGINE;   do SOURCES+=("$SRC/engine/$f.cpp"); done
for f in $RENDER;   do SOURCES+=("$SRC/render/$f.cpp"); done
for f in $TRON;     do SOURCES+=("$SRC/tron/$f.cpp"); done
for f in $UI;       do SOURCES+=("$SRC/ui/$f.cpp"); done
for f in $PARTICLES; do SOURCES+=("$SRC/thirdparty/particles/$f.cpp"); done

# Plain-C translation units, compiled separately so the C++ standard flag
# doesn't apply to them.
CSOURCES=("$SRC/thirdparty/binreloc/prefix.c" "$HERE/glu_shim.c" \
  "$HERE/gl_compat.c" "$HERE/audio_compat.c" "$HERE/sdl_compat.c")

# -- include paths (the -iquote dirs from Makefile.am, plus our config.h dir) --
INCLUDES=(
  -I"$HERE"                       # our config.h comes first
  -iquote "$SRC"
  -iquote "$SRC/tools"
  -iquote "$SRC/network"
  -iquote "$SRC/engine"
  -iquote "$SRC/render"
  -iquote "$SRC/tron"
  -iquote "$SRC/ui"
  -iquote "$SRC/thirdparty/particles"
  -iquote "$SRC/thirdparty/binreloc"
  -I"$WASM_DEPS_DIR/include/libxml2"
)

CXXFLAGS=(
  -std=c++17
  -O2
  -DHAVE_CONFIG_H
  -Wno-narrowing -Wno-writable-strings -Wno-deprecated -Wno-register
  -Wno-c++11-narrowing -fno-strict-aliasing
  -sUSE_SDL=1 -sUSE_SDL_IMAGE=1 -sUSE_SDL_MIXER=1 -sUSE_LIBPNG=1
  -sSDL2_IMAGE_FORMATS='["png"]'
)

LDFLAGS=(
  -O2
  -sUSE_SDL=1 -sUSE_SDL_IMAGE=1 -sUSE_SDL_MIXER=1 -sUSE_LIBPNG=1
  -sSDL2_IMAGE_FORMATS='["png"]'
  -sLEGACY_GL_EMULATION=1
  -sGL_UNSAFE_OPTS=0
  # JSPI suspends the native wasm stack for emscripten_sleep().  Unlike
  # Asyncify, it does not reconstruct a continuation through the engine's
  # deep virtual/function-pointer game loop (which trapped on a null function
  # when a match started). Chromium supports this API natively.
  -sJSPI=1
  -Wno-experimental
  # No ALLOW_MEMORY_GROWTH: a growable wasm memory is backed by a *resizable*
  # ArrayBuffer in current Chrome, and TextDecoder.decode() rejects those, so
  # every UTF8ToString -- including the path in each openat() -- throws and the
  # game cannot read its data files. We reserve the full heap up front instead.
  -sINITIAL_MEMORY=536870912
  -sTOTAL_STACK=16777216
  -sEXIT_RUNTIME=0
  -sEXPORTED_RUNTIME_METHODS='["callMain","FS","IDBFS"]'
  -lidbfs.js
  "$WASM_DEPS_DIR/lib/libxml2.a"
  --use-preload-plugins
  --preload-file "$HERE/data@/data"
  --shell-file "$HERE/shell.html"
)

# Compile the C sources to objects first (C standard, no -std=c++17).
COBJS=()
for c in "${CSOURCES[@]}"; do
  obj="$OUT/$(basename "${c%.*}").o"
  echo "Compiling (C) $(basename "$c")..."
  emcc -std=gnu11 -O2 -DHAVE_CONFIG_H "${INCLUDES[@]}" \
    -sUSE_SDL=1 -sUSE_SDL_MIXER=1 -c "$c" -o "$obj"
  COBJS+=("$obj")
done

echo "Compiling ${#SOURCES[@]} C++ translation units and linking..."
emcc "${CXXFLAGS[@]}" "${INCLUDES[@]}" "${LDFLAGS[@]}" \
  "${SOURCES[@]}" "${COBJS[@]}" \
  -o "$OUT/armagetronad.html"

# Emscripten names its outputs predictably. Add a per-build query string to
# the loader and every locateFile asset so Cloudflare/browser caches can never
# combine a JavaScript loader from one build with wasm/data from another.
BUILD_VERSION="$(date -u +%Y%m%d%H%M%S)"
sed -i "s/__BUILD_VERSION__/${BUILD_VERSION}/g; s/src=armagetronad\\.js/src=armagetronad.js?v=${BUILD_VERSION}/g" \
  "$OUT/armagetronad.html"

echo "Build complete: $OUT/armagetronad.html"
