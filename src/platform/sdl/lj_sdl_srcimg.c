/* lj_sdl_srcimg.c - Source-Images decoding (lj_sdl_srcimg.h). Every file is untrusted: SDL_LoadBMP validates
 * the header and pixel sizes; a NULL result or an oversized image falls back to our art. */
#include "platform/sdl/lj_sdl_srcimg.h"

#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "core/lj_str.h"
#include "gfx/lj_glyphs.h"

enum { LJ_SRCIMG_MAX_SIDE = 1024 };
static char lj_srcimg_root[1024];                    /* UTF-8, ends with a separator */

static int load_path(const char *path, lj_surface *out)
{
    SDL_Surface *bmp = SDL_LoadBMP(path), *c;
    int y;
    if (!bmp) return -1;
    c = bmp->w > 0 && bmp->h > 0 && bmp->w <= LJ_SRCIMG_MAX_SIDE && bmp->h <= LJ_SRCIMG_MAX_SIDE ? SDL_ConvertSurface(bmp, SDL_PIXELFORMAT_XRGB8888) : NULL;
    SDL_DestroySurface(bmp);
    if (!c) return -1;
    if (lj_surface_init(out, c->w, c->h) != 0) { SDL_DestroySurface(c); return -1; }
    for (y = 0; y < c->h; y++) memcpy(out->px + (size_t)y * (size_t)c->w, (const char *)c->pixels + (size_t)y * (size_t)c->pitch, (size_t)c->w * 4);
    for (y = 0; y < c->w * c->h; y++) out->px[y] &= 0xFFFFFFu;
    SDL_DestroySurface(c);
    return 0;
}

int lj_sdl_srcimg_load(const char *folder, const char *file, lj_surface *out)
{
    static const char *const ext[] = { "", ".bmp", ".BMP", ".Bmp" };
    char name[256], base[256], path[1600];
    size_t n;
    int i;
    if (!folder || !file || !*file || !out || !lj_srcimg_root[0] || strstr(file, "..")) return -1;   /* names come from show data: no climbing out */
    if (lj_cp1252_to_utf8(name, sizeof name, file) != 0) return -1;
    for (n = 0; name[n]; n++) if (name[n] == '\\') name[n] = '/';                                     /* LJ2 sub-folder names ("Martin\\Cone") */
    snprintf(base, sizeof base, "%s", name);
    if (n > 4 && SDL_strcasecmp(name + n - 4, ".bmp") == 0) base[n - 4] = 0;                          /* try the stem with each spelling of the extension */
    for (i = 0; i < 4; i++) {
        SDL_PathInfo info;
        snprintf(path, sizeof path, "%s%s/%s%s", lj_srcimg_root, folder, i ? base : name, ext[i]);
        if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE) return load_path(path, out);
    }
    return -1;
}

int lj_sdl_srcimg_init(const char *userdir)
{
    SDL_PathInfo info;
    size_t n;
    lj_srcimg_root[0] = 0;
    lj_srcimg_set_loader(NULL);
    if (!userdir) return -1;
    n = strlen(userdir);
    snprintf(lj_srcimg_root, sizeof lj_srcimg_root, "%s%sSource-Images/", userdir, n && (userdir[n - 1] == '/' || userdir[n - 1] == '\\') ? "" : "/");
    if (!SDL_GetPathInfo(lj_srcimg_root, &info) || info.type != SDL_PATHTYPE_DIRECTORY) { lj_srcimg_root[0] = 0; return 0; }
    lj_srcimg_set_loader(lj_sdl_srcimg_load);
    return 1;
}
