#!/bin/bash
# Emscripten build for Armagetron Advanced (release_0.2.9) -> WebAssembly.
#
# Compiles the client sources with emcc, mapping SDL/OpenGL to the browser's
# WebGL + Web Audio via emscripten's SDL1.2 ports, and packages the game data.
#
# Layout is discovered relative to this script; override with env vars:
#   EMSDK_DIR      path to an emsdk checkout   (default: <workspace>/emsdk)
#   WASM_DEPS_DIR  prefix with wasm libxml2    (default: <workspace>/wasm-deps)
#   WEB_SHELL_FILE Emscripten HTML shell (defaults to webbuild/shell.html)
#                  (default: <workspace>/armagetronad-platform/apps/web/armagetronad-shell.html)
# where <workspace> is the directory two levels above this script.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$HERE/../src" && pwd)"
WORKSPACE="$(cd "$HERE/../.." && pwd)"
PUBLIC_BUILD_ROOT="/__wasi__/wasmageddon"

EMSDK_DIR="${EMSDK_DIR:-$WORKSPACE/emsdk}"
WASM_DEPS_DIR="${WASM_DEPS_DIR:-$WORKSPACE/wasm-deps}"
PLATFORM_DIR="${ARMAGETRONAD_PLATFORM_DIR:-$WORKSPACE/armagetronad-platform}"
# The shell is part of the GPL client, not the platform: emcc compiles it into
# the distributed binary via --shell-file, and it carries the game's own canvas,
# input and touch controls. It lives here so the source a player is offered is
# the source their client was actually built from.
WEB_SHELL_FILE="${WEB_SHELL_FILE:-$HERE/shell.html}"
OUT="$HERE/dist"
mkdir -p "$OUT"

if [[ ! -f "$WEB_SHELL_FILE" ]]; then
  echo "Missing web shell: $WEB_SHELL_FILE" >&2
  echo "Expected webbuild/shell.html, or set WEB_SHELL_FILE to override." >&2
  exit 1
fi

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
  -ffile-prefix-map="$WORKSPACE=$PUBLIC_BUILD_ROOT"
  -ffile-prefix-map="$HOME=/__wasi__/home"
  -DHAVE_CONFIG_H
  # The engine throws across the simulation: eSensor::PassEdge unwinds out of a
  # wall walk with a throw, and main()/sg_EnterGame() guard themselves with
  # try/catch. Without this emscripten compiles __cxa_throw to abort(), so the
  # first cycle that sensed a wall killed the runtime. Emscripten (JS) EH rather
  # than -fwasm-exceptions, which does not combine with ASYNCIFY.
  -fexceptions
  -Wno-narrowing -Wno-writable-strings -Wno-deprecated -Wno-register
  -Wno-c++11-narrowing -fno-strict-aliasing
  -sUSE_SDL=1 -sUSE_SDL_IMAGE=1 -sUSE_SDL_MIXER=1 -sUSE_LIBPNG=1
  -sSDL2_IMAGE_FORMATS='["png"]'
)

LDFLAGS=(
  -O2
  -fexceptions
  -sUSE_SDL=1 -sUSE_SDL_IMAGE=1 -sUSE_SDL_MIXER=1 -sUSE_LIBPNG=1
  -sSDL2_IMAGE_FORMATS='["png"]'
  -sLEGACY_GL_EMULATION=1
  -sGL_UNSAFE_OPTS=0
  -sASYNCIFY=1
  # Entering a match crosses the engine's deep virtual/function-pointer call
  # graph before it reaches a frame yield.  The default 16 KiB unwind stack
  # corrupts that continuation and traps on a null table function.
  -sASYNCIFY_STACK_SIZE=65536
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
  --shell-file "$WEB_SHELL_FILE"
)

# Compile the C sources to objects first (C standard, no -std=c++17).
COBJS=()
for c in "${CSOURCES[@]}"; do
  obj="$OUT/$(basename "${c%.*}").o"
  echo "Compiling (C) $(basename "$c")..."
  emcc -std=gnu11 -O2 -DHAVE_CONFIG_H "${INCLUDES[@]}" \
    -ffile-prefix-map="$WORKSPACE=$PUBLIC_BUILD_ROOT" \
    -ffile-prefix-map="$HOME=/__wasi__/home" \
    -sUSE_SDL=1 -sUSE_SDL_MIXER=1 -c "$c" -o "$obj"
  COBJS+=("$obj")
done

echo "Compiling ${#SOURCES[@]} C++ translation units and linking..."
emcc "${CXXFLAGS[@]}" "${INCLUDES[@]}" "${LDFLAGS[@]}" \
  "${SOURCES[@]}" "${COBJS[@]}" \
  -o "$OUT/armagetronad.html"

rm -f "${COBJS[@]}"

# Emscripten names its outputs predictably. Add a per-build query string to
# the loader and every locateFile asset so Cloudflare/browser caches can never
# combine a JavaScript loader from one build with wasm/data from another.
BUILD_VERSION="$(date -u +%Y%m%d%H%M%S)"

# Bake the real asset sizes in so the loading bar can weight the two downloads
# against each other. The wasm is the larger half and emscripten reports no
# progress for it at all, so without this the bar only ever tracked the data
# package and appeared to stall for the rest of the load.
WASM_BYTES="$(stat -c%s "$OUT/armagetronad.wasm")"
DATA_BYTES="$(stat -c%s "$OUT/armagetronad.data" 2>/dev/null || echo 0)"

cp "$HERE/assets/wasmagetron-favicon-inverse-32.png" "$OUT/wasmagetron-favicon-32.png"
cp "$HERE/assets/wasmagetron-favicon-inverse-128.png" "$OUT/wasmagetron-favicon-128.png"
cp "$HERE/assets/wasmagetron-favicon-inverse-256.png" "$OUT/wasmagetron-favicon-256.png"
cp "$HERE/assets/wasmagetron-social-banner.png" "$OUT/wasmagetron-social-banner.png"

sed -i "s/__BUILD_VERSION__/${BUILD_VERSION}/g; \
        s/__WASM_BYTES__/${WASM_BYTES}/g; \
        s/__DATA_BYTES__/${DATA_BYTES}/g; \
        s/src=armagetronad\\.js/src=armagetronad.js?v=${BUILD_VERSION}/g" \
  "$OUT/armagetronad.html"

# Emscripten's preload packager bakes the local output path into bookkeeping
# strings such as addRunDependency("datafile_/home/.../armagetronad.data").
# They are not needed in the browser and should never ship in public artifacts.
PACKAGE_ABS="$OUT/armagetronad.data"
PACKAGE_ABS="$PACKAGE_ABS" perl -0pi -e 's/\Q$ENV{PACKAGE_ABS}\E/armagetronad.data/g' \
  "$OUT/armagetronad.js"

# Prebuilt static dependencies can still carry build-time absolute paths in
# wasm data segments. Use a same-length replacement so the binary remains valid.
if [[ ${#WORKSPACE} -eq ${#PUBLIC_BUILD_ROOT} ]]; then
  WORKSPACE="$WORKSPACE" PUBLIC_BUILD_ROOT="$PUBLIC_BUILD_ROOT" perl -0pi -e \
    's/\Q$ENV{WORKSPACE}\E/$ENV{PUBLIC_BUILD_ROOT}/g' \
    "$OUT/armagetronad.wasm"
fi

# Cloudflare Pages headers. Every asset URL carries ?v=<build>, so a given URL
# is immutable and can be cached hard -- that is what stops a returning player
# re-downloading ~6 MB for a build they already have. The HTML must not be
# cached, since it is what carries the new version string.
cat > "$OUT/_headers" <<'HEADERS'
/*
  Strict-Transport-Security: max-age=31536000; includeSubDomains; preload
  Content-Security-Policy: upgrade-insecure-requests; block-all-mixed-content
  X-Content-Type-Options: nosniff
  Referrer-Policy: strict-origin-when-cross-origin
/armagetronad.wasm
  Cache-Control: public, max-age=31536000, immutable
/armagetronad.data
  Cache-Control: public, max-age=31536000, immutable
/armagetronad.js
  Cache-Control: public, max-age=31536000, immutable
/wasmagetron-favicon-32.png
  Cache-Control: public, max-age=31536000, immutable
/wasmagetron-favicon-128.png
  Cache-Control: public, max-age=31536000, immutable
/wasmagetron-favicon-256.png
  Cache-Control: public, max-age=31536000, immutable
/wasmagetron-social-banner.png
  Cache-Control: public, max-age=31536000, immutable
/armagetronad.html
  Cache-Control: public, max-age=0, must-revalidate
/
  Cache-Control: public, max-age=0, must-revalidate
HEADERS

SHIPPED_FILES=(
  "$OUT/armagetronad.html"
  "$OUT/armagetronad.js"
  "$OUT/armagetronad.wasm"
  "$OUT/armagetronad.data"
  "$OUT/wasmagetron-favicon-32.png"
  "$OUT/wasmagetron-favicon-128.png"
  "$OUT/wasmagetron-favicon-256.png"
  "$OUT/wasmagetron-social-banner.png"
  "$OUT/_headers"
  "$OUT/_redirects"
  "$OUT/BUILD-INFO.json"
)

PII_PATTERNS=(
  "$HOME"
  "$WORKSPACE"
  "$(basename "$WORKSPACE")"
  "/home/${USER:-}"
  "/Users/${USER:-}"
)

for pattern in "${PII_PATTERNS[@]}"; do
  [[ -n "$pattern" ]] || continue
  if rg -a -F --quiet "$pattern" "${SHIPPED_FILES[@]}"; then
    echo "Build output contains local build identity/path: $pattern" >&2
    rg -a -F --files-with-matches "$pattern" "${SHIPPED_FILES[@]}" >&2 || true
    exit 1
  fi
done

echo "Build complete: $OUT/armagetronad.html"
echo "  wasm ${WASM_BYTES} bytes, data ${DATA_BYTES} bytes, version ${BUILD_VERSION}"
