/* lj_sdl.h - one platform call lj_platform.h does not carry: the surface read-back
 * the --selftest uses (design 2.8, W0 acceptance 3). SDL-free header; implemented
 * in lj_sdl.c beside lj_platform.h's functions. Popups, hints and the caption
 * buttons go through the lj_window_host callbacks lj_sdl.c installs on every
 * window it opens (ui/lj_window.h). */
#ifndef LJ_PLATFORM_SDL_SDL_H
#define LJ_PLATFORM_SDL_SDL_H

#include "gfx/lj_surface.h"
#include "ui/lj_window.h"

int lj_platform_readback(lj_window *win, lj_surface *out);   /* copies the OS window surface (what the screen shows) into a new lj_surface at zoom 1; 0 / -1 */

#endif /* LJ_PLATFORM_SDL_SDL_H */
