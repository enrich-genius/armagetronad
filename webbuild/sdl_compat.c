/*
 * SDL / SDL_image entry points declared by emscripten's headers but not
 * implemented by its SDL1 port.
 *
 *  - SDL_SetEventFilter : emscripten's libsdl.js has no event-filter hook.
 *                         Armagetron only uses it to swallow mouse-motion
 *                         events while the pointer is being warped, which the
 *                         browser does not do anyway.
 *  - IMG_InvertAlpha    : removed from SDL_image 2.x. The game calls it once to
 *                         ask for non-inverted alpha, which is already the
 *                         behaviour of the current loader.
 *  - SDL_ConvertSurface : emscripten implements this by drawing one surface's
 *                         backing <canvas> onto another. Surfaces the game
 *                         builds itself have no canvas, so the call throws a
 *                         DOM TypeError out of the middle of the wasm stack --
 *                         which is what killed the first cycle texture load on
 *                         entering a game. A C definition overrides the JS
 *                         library version, so copy the pixels instead.
 */
#include <SDL.h>
#include <string.h>


void SDL_SetEventFilter(SDL_EventFilter filter, void *userdata)
{
    (void)filter;
    (void)userdata;
}

int IMG_InvertAlpha(int on)
{
    (void)on;
    return 0;
}

/* emscripten's SDL1 always decodes images to 32-bit RGBA, and the only caller
 * (rSurface::CopyFrom) asks for the source's own format, so produce that
 * directly rather than honouring an arbitrary target format.
 *
 * The source is never locked: for a GL app IMG_Load already copies the decoded
 * pixels into the heap and then drops the surface's canvas and 2D context, so
 * locking would dereference a null ctx while ->pixels is already valid. The
 * destination is locked once to allocate that heap buffer and is deliberately
 * never unlocked -- emscripten's SDL_UnlockSurface asserts !SDL.GL and pushes
 * pixels back through a context this surface is not going to keep. The game
 * only ever reads these copies back as texture upload source data. */
SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, Uint32 flags)
{
    SDL_Surface *dst;
    int row, bytes;

    (void)fmt;
    if (!src || !src->pixels)
        return NULL;

    dst = SDL_CreateRGBSurface(flags, src->w, src->h, 32,
                               0x000000ffu, 0x0000ff00u, 0x00ff0000u, 0xff000000u);
    if (!dst)
        return NULL;

    if (SDL_LockSurface(dst) != 0 || !dst->pixels)
        return dst;

    bytes = src->w * 4;
    if (bytes > dst->pitch) bytes = dst->pitch;
    if (bytes > src->pitch) bytes = src->pitch;
    for (row = 0; row < src->h; ++row)
        memcpy((Uint8 *)dst->pixels + (size_t)row * dst->pitch,
               (const Uint8 *)src->pixels + (size_t)row * src->pitch,
               (size_t)bytes);

    return dst;
}
