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
 */
#include <SDL.h>

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
