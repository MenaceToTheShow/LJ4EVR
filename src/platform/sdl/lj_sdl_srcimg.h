/* lj_sdl_srcimg.h - the Source-Images override (design 7.8, W4-SRCIMG): when the owner's own copy of
 * LightJockey 2's <userdata>/Source-Images/ folder exists, its Glyphs, Fixtures, Colors and Gobos BMPs are
 * decoded here with SDL_LoadBMP and handed to gfx (lj_srcimg_set_loader), which substitutes them for our art
 * by LJ2 file name. Nothing from that folder is shipped or committed. SDL-free header. */
#ifndef LJ_PLATFORM_SDL_SDL_SRCIMG_H
#define LJ_PLATFORM_SDL_SDL_SRCIMG_H

#include "gfx/lj_surface.h"

int lj_sdl_srcimg_init(const char *userdir);   /* userdir = the pref path (UTF-8 bytes, ME-65, trailing separator optional); 1 = folder found and loader set, 0 = absent (our art), -1 = bad argument */
int lj_sdl_srcimg_load(const char *folder, const char *file, lj_surface *out);   /* the loader: <root>/<folder>/<file> (CP1252 name; .bmp/.BMP/.Bmp tried); 0 / -1 (absent, malformed, over 1024 px a side) */

#endif /* LJ_PLATFORM_SDL_SDL_SRCIMG_H */
