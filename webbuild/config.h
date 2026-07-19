// Armagetron Advanced -- Emscripten/WebAssembly build configuration.
// Hand-written replacement for the autoconf-generated config.h, tailored to
// the emscripten toolchain (SDL1.2/SDL_image/SDL_mixer ports, WebGL via the
// legacy GL emulation, single-threaded main loop driven by the browser).

#ifndef ARMAGETRON_CONFIG_H_WASM
#define ARMAGETRON_CONFIG_H_WASM

#define STDC_HEADERS 1
#define HAVE_UNISTD_H 1
#define HAVE_STDLIB_H 1

// Disable the custom debugging memory manager: its malloc/free/realloc macro
// poisoning collides with modern libc++ headers, and the browser provides its
// own allocator. This makes tNEW() plain new and real_malloc() plain malloc.
#define DONTUSEMEMMANAGER 1

// Libraries provided by emscripten ports.
#define HAVE_LIBSDL 1
#define HAVE_LIBSDL_IMAGE 1
#define HAVE_SDL_SDL_IMAGE_H 1
#define HAVE_LIBSDL_MIXER 1
#define HAVE_LIBGL 1
#define HAVE_LIBGLU 1
#define HAVE_LIBPNG 1
#define HAVE_LIBZ 1
#define HAVE_LIBM 1

// GL / SDL header layout: emscripten installs SDL headers on the include path
// directly (<SDL.h>) and provides <SDL_opengl.h>.
#define HAVE_SDL_OPENGL_H 1

// libc math single-precision variants are all present in emscripten's musl-like libc.
#define HAVE_SINF 1
#define HAVE_COSF 1
#define HAVE_TANF 1
#define HAVE_ATAN2F 1
#define HAVE_SQRTF 1
#define HAVE_LOGF 1
#define HAVE_EXPF 1
#define HAVE_FABSF 1
#define HAVE_FLOORF 1
#define HAVE_WMEMSET 1

// misc libc features
#define HAVE_SELECT 1
#define HAVE_ISBLANK 1
#define HAVE_SOCKLEN_T 1

// No worker/pthread based background threads: the net-sync helper thread is
// already compiled out in rSysdep.cpp, and the frame loop runs on the browser
// main thread. Leaving HAVE_PTHREAD / HAVE_LIBZTHREAD undefined selects the
// single-threaded code paths.

// libxml2 is the modern one with the pib-create quirk absent.
// (HAVE_LIBXML2_WO_PIBCREATE intentionally left undefined.)

// ---------------------------------------------------------------------------
// Directory layout inside the emscripten virtual filesystem.
//   /data     : read-only game assets, baked in via --preload-file (MEMFS)
//   /persist  : writable user data, mounted on IDBFS so settings survive reloads
// ---------------------------------------------------------------------------
#define DATA_DIR            "/data"
#define CONFIG_DIR          "/data/config"
#define RESOURCE_DIR        "/data/resource"
#define INCLUDEDRESOURCE_DIR "/data/resource/included"

// Relocatable install paths normally emitted by the autoconf universal-variable
// step; here they just point back into the virtual FS asset tree.
#define AA_DATADIR          "/data"
#define AA_SYSCONFDIR       "/data/config"

#define USER_DATA_DIR       "/persist"
#define USER_CONFIG_DIR     "/persist/config"
#define VAR_DIR             "/persist/var"
#define AUTORESOURCE_DIR    "/persist/resource"
#define SCREENSHOT_DIR      "/persist/screenshots"

#define PROGNAME "armagetronad"

#define VERSION "0.2.9-web"

#endif // ARMAGETRON_CONFIG_H_WASM
