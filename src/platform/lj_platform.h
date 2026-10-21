/* lj_platform.h - the SDL-free interface app/ calls into platform/sdl (design 2.8).
 * Implemented by platform/sdl/lj_sdl.c, the only directory that includes
 * <SDL3/...>. This header itself includes no SDL so app/ code can be read and
 * compiled without it. Strings crossing here are CP1252; the platform converts
 * to UTF-8 at the OS edge (design 2.1). */
#ifndef LJ_PLATFORM_PLATFORM_H
#define LJ_PLATFORM_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include "gfx/lj_surface.h"
#include "ui/lj_window.h"

/* Open-window flags; the ownership rule of design 2.8 is applied to every
 * non-main window (parent pointer = main window, no SDL_WINDOW_UTILITY). */
#define LJ_WF_MAIN         1u      /* the application's main window: taskbar button, Alt+Tab entry; exactly one */
#define LJ_WF_STAY_ON_TOP  2u      /* fsStayOnTop -> SDL_WINDOW_ALWAYS_ON_TOP */
#define LJ_WF_NATIVE_FRAME 4u      /* [CHROME] Native=1: SDL's bordered window, the surface is client-only */
#define LJ_WF_HIDDEN       8u      /* create without showing */

typedef struct { int tick_ms; int zoom; int native_frame; } lj_platform_cfg;   /* repaint timer (100), [CHROME] Zoom (1|2), [CHROME] Native */

int  lj_platform_init(const lj_platform_cfg *cfg);                       /* SDL_Init(VIDEO|EVENTS|AUDIO); 0 / -1 + log */
int  lj_platform_open_window(lj_window *win, unsigned flags);            /* creates the SDL window for an lj_window, stores the handle with lj_window_set_native; 0 / -1 */
void lj_platform_close_window(lj_window *win);                           /* destroys the SDL window; the lj_window stays valid */
void lj_platform_show_window(lj_window *win, int show);
void lj_platform_move_window(lj_window *win, int x, int y);              /* screen position of the frame's top-left */
int  lj_platform_window_pos(const lj_window *win, int *x, int *y);
void lj_platform_present(lj_window *win);                                /* lj_window_paint + SDL_UpdateWindowSurfaceRects of lj_window_dirty() (scaled by zoom) */
int  lj_platform_run(void (*idle)(void *ud), void *ud);                  /* the UI loop: SDL_WaitEventTimeout(10), events -> lj_window_event, idle(ud) every wake, repaint timer every tick_ms; returns when lj_platform_quit() was called */
void lj_platform_quit(void);                                             /* makes lj_platform_run() return after the current pass */
void lj_platform_shutdown(void);                                         /* SDL_Quit */

int  lj_platform_load_bmp(const char *path, lj_surface *out);            /* SDL_LoadBMP -> 0x00RRGGBB lj_surface (owned by the caller); path CP1252; 0 / -1 + log */
int  lj_platform_clipboard_get(char *dst, size_t cap);                   /* CP1252 text, unmappable -> '?'; 0 / -1 when empty or unavailable */
int  lj_platform_clipboard_set(const char *text);                        /* CP1252 in; 0 / -1 */
int  lj_platform_pref_path(char *dst, size_t cap);                       /* SDL_GetPrefPath("LJ4EVR", "LJ4EVR") as UTF-8 bytes (not converted: ME-65, a path only fopen sees), trailing separator; 0 / -1 */
void lj_platform_sleep_us(uint64_t us);                                  /* SDL_DelayPrecise; the engine thread's sleep_until (design 3.1) */

/* Engine thread (design 3.1): started once after lj_engine_bind; stopped before lj_engine_destroy. */
struct lj_engine;
int  lj_platform_engine_start(struct lj_engine *e, uint32_t tick_us);    /* 0 / -1 */
void lj_platform_engine_stop(void);                                      /* joins */

#endif /* LJ_PLATFORM_PLATFORM_H */
