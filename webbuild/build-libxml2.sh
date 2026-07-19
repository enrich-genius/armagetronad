#!/bin/bash
# Cross-compile libxml2 to WebAssembly, producing the static library that
# build.sh links against.
#
# Armagetron uses libxml2 to parse map/resource XML. HTTP, FTP, threads,
# compression and the ICU/iconv converters are all disabled: none are reachable
# in the browser build, and leaving them in either fails to link or drags in
# dependencies that would have to be cross-compiled too.
#
# Paths are discovered relative to this script; override with env vars:
#   WASM_DEPS_DIR    install prefix          (default: <workspace>/wasm-deps)
#   LIBXML2_VERSION  version to build        (default: 2.12.10)
#   BUILD_DIR        scratch directory       (default: <workspace>)
# where <workspace> is the directory two levels above this script.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(cd "$HERE/../.." && pwd)"

WASM_DEPS_DIR="${WASM_DEPS_DIR:-$WORKSPACE/wasm-deps}"
LIBXML2_VERSION="${LIBXML2_VERSION:-2.12.10}"
BUILD_DIR="${BUILD_DIR:-$WORKSPACE}"

if [ -f "$WASM_DEPS_DIR/lib/libxml2.a" ]; then
  echo "libxml2 already built at $WASM_DEPS_DIR/lib/libxml2.a - nothing to do."
  exit 0
fi

if ! command -v emconfigure >/dev/null 2>&1; then
  echo "error: emconfigure not found. Activate emsdk first (source emsdk_env.sh)." >&2
  exit 1
fi

SERIES="${LIBXML2_VERSION%.*}"   # e.g. 2.12.10 -> 2.12
SRC="$BUILD_DIR/libxml2-$LIBXML2_VERSION"

if [ ! -d "$SRC" ]; then
  echo "Fetching libxml2 $LIBXML2_VERSION..."
  mkdir -p "$BUILD_DIR"
  curl -fsSL \
    "https://download.gnome.org/sources/libxml2/$SERIES/libxml2-$LIBXML2_VERSION.tar.xz" \
    -o "$BUILD_DIR/libxml2-$LIBXML2_VERSION.tar.xz"
  tar -xf "$BUILD_DIR/libxml2-$LIBXML2_VERSION.tar.xz" -C "$BUILD_DIR"
fi

cd "$SRC"
emconfigure ./configure \
  --host=wasm32-unknown-emscripten \
  --prefix="$WASM_DEPS_DIR" \
  --disable-shared --enable-static \
  --without-python --without-http --without-ftp --without-threads \
  --without-zlib --without-lzma --without-modules \
  --without-iconv --without-icu --without-debug

emmake make -j"$(nproc 2>/dev/null || echo 2)"
emmake make install

echo "libxml2 installed to $WASM_DEPS_DIR"
