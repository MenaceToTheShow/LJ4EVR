/* lj_sdl.c - platform/sdl: the only file that includes <SDL3/...> (design 2.8, 7.1, 7.5, 7.9, 3.1).
 * One SDL_Window per lj_window, borderless by default with the classic frame
 * drawn by ui/ and moved/resized through SDL_SetWindowHitTest over
 * lj_window_hit(); [CHROME] Native=1 switches to SDL's bordered windows. Every
 * non-main window carries SDL_PROP_WINDOW_CREATE_PARENT_POINTER = the main
 * window and never SDL_WINDOW_UTILITY (ME-7): one taskbar button, controls
 * hide with the main window. Dirty rects go out with
 * SDL_UpdateWindowSurfaceRects, scaled x2 with SDL_BlitSurfaceScaled NEAREST
 * under [CHROME] Zoom=2. Events become lj_ui_event with x,y in window-surface
 * pixels (zoom removed). Popup menus are SDL_CreatePopupWindow(POPUP_MENU)
 * windows shown through the lj_window_host callbacks. Strings cross here as
 * CP1252 and leave as UTF-8 (design 2.1). */
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "platform/lj_platform.h"
#include "platform/sdl/lj_sdl.h"
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "io/lj_artnet.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "engine/lj_engine.h"
#include "io/lj_sacn.h"

#define LJ_SDL_MAX_WINDOWS 256                 /* 208 forms + code-built windows */
#define LJ_SDL_MAX_DIRTY   32
#define LJ_SDL_PROP_WINDOW "lj4evr.window"     /* SDL_Window property -> lj_window* (a top-level window) */
#define LJ_SDL_PROP_OWNER  "lj4evr.owner"      /* SDL_Window property -> lj_window* owning a popup */

typedef struct { SDL_Window *sdl; lj_window *win; char title[128]; } lj_sdl_win;

/* ponytail: the platform's one static. lj_platform_quit() and the SDL callbacks
 * carry no handle, so the OS edge keeps its state here (the log sink rule's
 * sibling); nothing outside this file sees it. */
static struct {
    lj_platform_cfg cfg;
    SDL_Window *main;                                  /* parent pointer of every other window */
    lj_sdl_win *wins[LJ_SDL_MAX_WINDOWS]; int nwins;
    Uint32 ev_timer;                                   /* SDL_RegisterEvents user event: the repaint tick */
    SDL_TimerID timer;
    int quit;
    lj_window_host host;
    SDL_Thread *thread; SDL_AtomicInt stop; struct lj_engine *engine; uint32_t tick_us;
} lj_sdl;

/* ---- helpers ------------------------------------------------------------- */

static lj_window *lj_sdl_prop(SDL_Window *w, const char *name)
{
    return w ? (lj_window *)SDL_GetPointerProperty(SDL_GetWindowProperties(w), name, NULL) : NULL;
}

static lj_sdl_win *lj_sdl_pw(const lj_window *win)
{
    return win ? (lj_sdl_win *)lj_window_native(win) : NULL;
}

/* Frame pixels around the client: surface size minus client rect (0 under a native frame). */
static void lj_sdl_frame_extra(lj_window *win, int *dw, int *dh)
{
    lj_rect cr; const lj_surface *s = lj_window_surface(win);
    lj_window_client_rect(win, &cr);
    *dw = s->w - cr.w;
    *dh = s->h - cr.h;
}

/* Blits an lj_surface (0x00RRGGBB) into an SDL window surface, zoomed; rects in lj pixels, NULL = whole. */
static int lj_sdl_blit(SDL_Window *w, const lj_surface *s, const lj_rect *r, int n)
{
    SDL_Surface *dst = SDL_GetWindowSurface(w), *src;
    SDL_Rect rects[LJ_SDL_MAX_DIRTY];
    lj_rect all = { 0, 0, s->w, s->h };
    int i, z = lj_sdl.cfg.zoom;
    if (!dst) { lj_log("SDL_GetWindowSurface: %s", SDL_GetError()); return -1; }
    src = SDL_CreateSurfaceFrom(s->w, s->h, SDL_PIXELFORMAT_XRGB8888, s->px, s->w * 4);
    if (!src) { lj_log("SDL_CreateSurfaceFrom: %s", SDL_GetError()); return -1; }
    if (!r || n <= 0 || n > LJ_SDL_MAX_DIRTY) { r = &all; n = 1; }
    for (i = 0; i < n; i++) {
        SDL_Rect sr = { r[i].x, r[i].y, r[i].w, r[i].h };
        rects[i].x = r[i].x * z; rects[i].y = r[i].y * z; rects[i].w = r[i].w * z; rects[i].h = r[i].h * z;
        if (z == 1) SDL_BlitSurface(src, &sr, dst, &rects[i]);
        else SDL_BlitSurfaceScaled(src, &sr, dst, &rects[i], SDL_SCALEMODE_NEAREST);
    }
    SDL_DestroySurface(src);
    return SDL_UpdateWindowSurfaceRects(w, rects, n) ? 0 : -1;
}

static Uint32 SDLCALL lj_sdl_timer_cb(void *ud, SDL_TimerID id, Uint32 interval)
{
    SDL_Event e;
    (void)ud; (void)id;
    memset(&e, 0, sizeof e);
    e.type = lj_sdl.ev_timer;
    SDL_PushEvent(&e);
    return interval;
}

/* Self-drawn frame: caption drags, sizeable borders resize (design 7.5); everything else reaches lj_window_event. */
static SDL_HitTestResult SDLCALL lj_sdl_hit(SDL_Window *w, const SDL_Point *p, void *ud)
{
    static const SDL_HitTestResult map[] = {
        SDL_HITTEST_NORMAL, SDL_HITTEST_DRAGGABLE, SDL_HITTEST_NORMAL, SDL_HITTEST_NORMAL, SDL_HITTEST_NORMAL, SDL_HITTEST_NORMAL,
        SDL_HITTEST_RESIZE_TOPLEFT, SDL_HITTEST_RESIZE_TOP, SDL_HITTEST_RESIZE_TOPRIGHT, SDL_HITTEST_RESIZE_RIGHT,
        SDL_HITTEST_RESIZE_BOTTOMRIGHT, SDL_HITTEST_RESIZE_BOTTOM, SDL_HITTEST_RESIZE_BOTTOMLEFT, SDL_HITTEST_RESIZE_LEFT };
    int h = (int)lj_window_hit(ud, p->x / lj_sdl.cfg.zoom, p->y / lj_sdl.cfg.zoom);
    (void)w;
    return h >= 0 && h < LJ_COUNT_OF(map) ? map[h] : SDL_HITTEST_NORMAL;
}

/* SDL keycode/scancode -> Windows virtual-key code (the numbers LJ2's .Hot files carry). Layout-independent via scancodes. */
static int lj_sdl_vk(SDL_Keycode k, SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return 'A' + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return '1' + (sc - SDL_SCANCODE_1);
    if (sc == SDL_SCANCODE_0) return '0';
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return 0x70 + (sc - SDL_SCANCODE_F1);
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) return 0x61 + (sc - SDL_SCANCODE_KP_1);
    switch (sc) {
    case SDL_SCANCODE_KP_0: return 0x60;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x6A;
    case SDL_SCANCODE_KP_PLUS: return 0x6B;
    case SDL_SCANCODE_KP_MINUS: return 0x6D;
    case SDL_SCANCODE_KP_PERIOD: return 0x6E;
    case SDL_SCANCODE_KP_DIVIDE: return 0x6F;
    case SDL_SCANCODE_SEMICOLON: return 0xBA;
    case SDL_SCANCODE_EQUALS: return 0xBB;
    case SDL_SCANCODE_COMMA: return 0xBC;
    case SDL_SCANCODE_MINUS: return 0xBD;
    case SDL_SCANCODE_PERIOD: return 0xBE;
    case SDL_SCANCODE_SLASH: return 0xBF;
    case SDL_SCANCODE_GRAVE: return 0xC0;
    case SDL_SCANCODE_LEFTBRACKET: return 0xDB;
    case SDL_SCANCODE_BACKSLASH: return 0xDC;
    case SDL_SCANCODE_RIGHTBRACKET: return 0xDD;
    case SDL_SCANCODE_APOSTROPHE: return 0xDE;
    default: break;
    }
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return 0x0D;
    case SDLK_ESCAPE: return 0x1B;
    case SDLK_TAB: return 0x09;
    case SDLK_SPACE: return 0x20;
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_DELETE: return 0x2E;
    case SDLK_INSERT: return 0x2D;
    case SDLK_HOME: return 0x24;
    case SDLK_END: return 0x23;
    case SDLK_PAGEUP: return 0x21;
    case SDLK_PAGEDOWN: return 0x22;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    case SDLK_LALT: case SDLK_RALT: return 0x12;
    case SDLK_PAUSE: return 0x13;
    case SDLK_CAPSLOCK: return 0x14;
    case SDLK_NUMLOCKCLEAR: return 0x90;
    case SDLK_SCROLLLOCK: return 0x91;
    case SDLK_PRINTSCREEN: return 0x2C;
    default: return 0;
    }
}

static void lj_sdl_mods(SDL_Keymod m, unsigned *shift, unsigned *ctrl, unsigned *alt)
{
    *shift = (m & SDL_KMOD_SHIFT) != 0;
    *ctrl  = (m & SDL_KMOD_CTRL) != 0;
    *alt   = (m & SDL_KMOD_ALT) != 0;
}

static void lj_sdl_mouse(lj_ui_event *ev, int type, int x, int y)
{
    unsigned s, c, a;
    ev->type = type;
    ev->u.mouse.type = type;
    ev->u.mouse.x = x / lj_sdl.cfg.zoom;
    ev->u.mouse.y = y / lj_sdl.cfg.zoom;
    lj_sdl_mods(SDL_GetModState(), &s, &c, &a);
    ev->u.mouse.shift = s; ev->u.mouse.ctrl = c; ev->u.mouse.alt = a;
}

static void lj_sdl_dispatch(const SDL_Event *e)
{
    lj_ui_event ev;
    lj_window *win, *owner;
    SDL_Window *sw;
    int i;

    if (e->type == lj_sdl.ev_timer) {
        memset(&ev, 0, sizeof ev);
        ev.type = LJ_EV_TIMER;
        for (i = 0; i < lj_sdl.nwins; i++) lj_window_event(lj_sdl.wins[i]->win, &ev);
        return;
    }
    if (e->type == SDL_EVENT_QUIT) { lj_sdl.quit = 1; return; }
    sw = SDL_GetWindowFromEvent(e);
    win = lj_sdl_prop(sw, LJ_SDL_PROP_WINDOW);
    owner = win ? NULL : lj_sdl_prop(sw, LJ_SDL_PROP_OWNER);
    if (!win && !owner) return;
    memset(&ev, 0, sizeof ev);
    switch (e->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        lj_sdl_mouse(&ev, e->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? LJ_EV_MOUSE_DOWN : LJ_EV_MOUSE_UP, (int)e->button.x, (int)e->button.y);
        ev.u.mouse.button = e->button.button == SDL_BUTTON_LEFT ? 1 : e->button.button == SDL_BUTTON_RIGHT ? 2 : 3;
        ev.u.mouse.clicks = e->button.clicks >= 2 ? 2 : 1;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        lj_sdl_mouse(&ev, LJ_EV_MOUSE_MOVE, (int)e->motion.x, (int)e->motion.y);
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        lj_sdl_mouse(&ev, LJ_EV_MOUSE_WHEEL, (int)e->wheel.mouse_x, (int)e->wheel.mouse_y);
        ev.u.mouse.wheel = e->wheel.integer_y;
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        ev.type = LJ_EV_MOUSE_LEAVE; ev.u.mouse.type = LJ_EV_MOUSE_LEAVE;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        unsigned s, c, a;
        ev.type = e->type == SDL_EVENT_KEY_DOWN ? LJ_EV_KEY_DOWN : LJ_EV_KEY_UP;
        ev.u.key.type = ev.type;
        ev.u.key.key = lj_sdl_vk(e->key.key, e->key.scancode);
        lj_sdl_mods(e->key.mod, &s, &c, &a);
        ev.u.key.shift = s; ev.u.key.ctrl = c; ev.u.key.alt = a;
        break; }
    case SDL_EVENT_TEXT_INPUT: {
        char cp[64]; const unsigned char *p;
        if (!win) return;
        lj_utf8_to_cp1252(cp, sizeof cp, e->text.text);
        ev.type = LJ_EV_CHAR; ev.u.key.type = LJ_EV_CHAR;
        for (p = (const unsigned char *)cp; *p; p++) { ev.u.key.ch = *p; lj_window_event(win, &ev); }
        return; }
    case SDL_EVENT_WINDOW_FOCUS_GAINED: if (!win) return; lj_window_set_active(win, 1); ev.type = LJ_EV_FOCUS_IN; break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:   if (!win) return; lj_window_set_active(win, 0); ev.type = LJ_EV_FOCUS_OUT; break;
    case SDL_EVENT_WINDOW_EXPOSED:      if (!win) return; lj_window_invalidate_all(win); ev.type = LJ_EV_EXPOSE; break;
    case SDL_EVENT_WINDOW_RESIZED: {
        int dw, dh, w = e->window.data1 / lj_sdl.cfg.zoom, h = e->window.data2 / lj_sdl.cfg.zoom;
        if (!win) return;
        lj_sdl_frame_extra(win, &dw, &dh);
        ev.type = LJ_EV_RESIZE; ev.u.size.w = w - dw; ev.u.size.h = h - dh;
        if (lj_window_resize(win, ev.u.size.w, ev.u.size.h) != 0) return;
        lj_window_invalidate_all(win);
        break; }
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: if (!win) return; ev.type = LJ_EV_CLOSE; break;
    default: return;
    }
    if (win) lj_window_event(win, &ev);
    else if (ev.type <= LJ_EV_MOUSE_LEAVE) lj_window_popup_event(owner, sw, &ev);   /* popup coords stay relative to the popup */
}

/* ---- lj_window_host: popups, screen geometry, caption buttons (ui/lj_window.h) ---- */

static int lj_sdl_host_screen_pos(void *ud, const lj_window *w, int *x, int *y)
{
    (void)ud;
    return lj_platform_window_pos(w, x, y);
}

static void *lj_sdl_host_popup_show(void *ud, lj_window *owner, const lj_surface *s, int sx, int sy)
{
    lj_sdl_win *pw = lj_sdl_pw(owner);
    SDL_Window *p;
    int ox, oy, z = lj_sdl.cfg.zoom;
    (void)ud;
    if (!pw || !s || s->w <= 0 || s->h <= 0 || !SDL_GetWindowPosition(pw->sdl, &ox, &oy)) return NULL;
    p = SDL_CreatePopupWindow(pw->sdl, sx * z - ox, sy * z - oy, s->w * z, s->h * z, SDL_WINDOW_POPUP_MENU);
    if (!p) { lj_log("SDL_CreatePopupWindow: %s", SDL_GetError()); return NULL; }
    SDL_SetPointerProperty(SDL_GetWindowProperties(p), LJ_SDL_PROP_OWNER, owner);
    lj_sdl_blit(p, s, NULL, 0);
    return p;
}

static void lj_sdl_host_popup_update(void *ud, void *handle, const lj_surface *s)
{
    (void)ud;
    if (handle && s) lj_sdl_blit(handle, s, NULL, 0);
}

static void lj_sdl_host_popup_hide(void *ud, void *handle)
{
    (void)ud;
    if (handle) SDL_DestroyWindow(handle);
}

static lj_window *lj_sdl_host_window_at(void *ud, int sx, int sy, int *x, int *y)
{
    int i, pass, wx, wy, ww, wh, z = lj_sdl.cfg.zoom;
    (void)ud;
    for (pass = 0; pass < 2; pass++)                 /* fsStayOnTop windows first: they are above every normal window */
    for (i = lj_sdl.nwins - 1; i >= 0; i--) {      /* ponytail: then last opened wins; no z-order query in SDL */
        SDL_Window *w = lj_sdl.wins[i]->sdl;
        SDL_WindowFlags f = SDL_GetWindowFlags(w);
        if ((f & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) || ((f & SDL_WINDOW_ALWAYS_ON_TOP) != 0) == pass) continue;   /* pass 0: on-top only, pass 1: the rest */
        if (!SDL_GetWindowPosition(w, &wx, &wy) || !SDL_GetWindowSize(w, &ww, &wh)) continue;
        if (sx >= wx && sx < wx + ww && sy >= wy && sy < wy + wh) {
            if (x) *x = (sx - wx) / z;
            if (y) *y = (sy - wy) / z;
            return lj_sdl.wins[i]->win;
        }
    }
    return NULL;
}

static void lj_sdl_host_sys_cmd(void *ud, lj_window *w, int cmd)
{
    lj_sdl_win *pw = lj_sdl_pw(w);
    lj_ui_event ev;
    (void)ud;
    if (!pw) return;
    switch (cmd) {
    case LJ_SYS_RESTORE:  SDL_RestoreWindow(pw->sdl); break;
    case LJ_SYS_MINIMIZE: SDL_MinimizeWindow(pw->sdl); break;
    case LJ_SYS_MAXIMIZE: SDL_MaximizeWindow(pw->sdl); break;
    case LJ_SYS_CLOSE:    memset(&ev, 0, sizeof ev); ev.type = LJ_EV_CLOSE; lj_window_event(w, &ev); break;
    default: break;       /* Move / Size by keyboard: the hit-test drag covers the mouse path; W1 if wanted */
    }
}

/* ---- lj_platform.h --------------------------------------------------------- */

int lj_platform_init(const lj_platform_cfg *cfg)
{
    memset(&lj_sdl, 0, sizeof lj_sdl);
    if (cfg) lj_sdl.cfg = *cfg;
    if (lj_sdl.cfg.zoom != 2) lj_sdl.cfg.zoom = 1;
    if (lj_sdl.cfg.tick_ms <= 0) lj_sdl.cfg.tick_ms = 100;
    lj_sdl.host.screen_pos = lj_sdl_host_screen_pos;
    lj_sdl.host.popup_show = lj_sdl_host_popup_show;
    lj_sdl.host.popup_update = lj_sdl_host_popup_update;
    lj_sdl.host.popup_hide = lj_sdl_host_popup_hide;
    lj_sdl.host.window_at = lj_sdl_host_window_at;
    lj_sdl.host.sys_cmd = lj_sdl_host_sys_cmd;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { lj_log("SDL_Init: %s", SDL_GetError()); return -1; }   /* audio is W5 */
    lj_sdl.ev_timer = SDL_RegisterEvents(1);
    lj_sdl.timer = SDL_AddTimer((Uint32)lj_sdl.cfg.tick_ms, lj_sdl_timer_cb, NULL);
    if (!lj_sdl.ev_timer || !lj_sdl.timer) { lj_log("SDL timer: %s", SDL_GetError()); SDL_Quit(); return -1; }
    return 0;
}

int lj_platform_open_window(lj_window *win, unsigned flags)
{
    const lj_form_desc *d;
    const lj_window_caption_state *cs;
    const lj_surface *s;
    lj_sdl_win *pw;
    SDL_PropertiesID props;
    SDL_Window *sdl;
    char title[512];
    int z = lj_sdl.cfg.zoom, native, sizeable, dw, dh;

    if (!win || lj_window_native(win)) return -1;
    if (lj_sdl.nwins >= LJ_SDL_MAX_WINDOWS) { lj_log("platform: %d windows open", lj_sdl.nwins); return -1; }
    if (!(flags & LJ_WF_MAIN) && !lj_sdl.main) { lj_log("platform: the main window must open first"); return -1; }
    d = lj_window_desc(win);
    native = (flags & LJ_WF_NATIVE_FRAME) || lj_sdl.cfg.native_frame;
    lj_window_set_native_frame(win, native);
    lj_window_set_host(win, &lj_sdl.host);
    lj_window_invalidate_all(win);
    lj_window_paint(win);                                   /* the surface now has its final size */
    s = lj_window_surface(win);
    cs = lj_window_caption(win);
    sizeable = cs->border == LJ_BS_SIZEABLE || cs->border == LJ_BS_SIZETOOLWIN;
    if (d && d->style == LJ_FS_STAYONTOP) flags |= LJ_WF_STAY_ON_TOP;
    lj_cp1252_to_utf8(title, sizeof title, cs->text);

    props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, s->w * z);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, s->h * z);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, !native);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, sizeable != 0);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, (flags & LJ_WF_HIDDEN) != 0);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, (flags & LJ_WF_STAY_ON_TOP) != 0);
    /* ME-7: owner = the main window, never SDL_WINDOW_UTILITY; never SDL_WINDOW_HIGH_PIXEL_DENSITY (design 7.1). */
    if (!(flags & LJ_WF_MAIN)) SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_PARENT_POINTER, lj_sdl.main);
    sdl = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!sdl) { lj_log("SDL_CreateWindow %s: %s", cs->text, SDL_GetError()); return -1; }

    pw = calloc(1, sizeof *pw);
    if (!pw) lj_fatal("platform: out of memory");
    pw->sdl = sdl; pw->win = win;
    lj_str_copy(pw->title, sizeof pw->title, cs->text);
    lj_window_set_native(win, pw);
    SDL_SetPointerProperty(SDL_GetWindowProperties(sdl), LJ_SDL_PROP_WINDOW, win);
    if (!native) SDL_SetWindowHitTest(sdl, lj_sdl_hit, win);
    lj_sdl_frame_extra(win, &dw, &dh);
    if (d && (d->min_width > 0 || d->min_height > 0))
        SDL_SetWindowMinimumSize(sdl, d->min_width * z, d->min_height * z);   /* Constraints are outer sizes (lj_window_resize clamps client + frame against them); adding dw/dh made TCurrentCue 620 wide (W1 acceptance 10) */
    SDL_StartTextInput(sdl);
    if (flags & LJ_WF_MAIN) lj_sdl.main = sdl;
    lj_sdl.wins[lj_sdl.nwins++] = pw;
    lj_window_invalidate_all(win);
    lj_platform_present(win);
    return 0;
}

void lj_platform_close_window(lj_window *win)
{
    lj_sdl_win *pw = lj_sdl_pw(win);
    int i;
    if (!pw) return;
    for (i = 0; i < lj_sdl.nwins; i++)
        if (lj_sdl.wins[i] == pw) { lj_sdl.wins[i] = lj_sdl.wins[--lj_sdl.nwins]; break; }
    if (pw->sdl == lj_sdl.main) lj_sdl.main = NULL;
    SDL_DestroyWindow(pw->sdl);                              /* its popups go with it */
    lj_window_set_native(win, NULL);
    lj_window_set_host(win, NULL);
    free(pw);
}

void lj_platform_show_window(lj_window *win, int show)
{
    lj_sdl_win *pw = lj_sdl_pw(win);
    if (!pw) return;
    if (show) SDL_ShowWindow(pw->sdl); else SDL_HideWindow(pw->sdl);
}

void lj_platform_move_window(lj_window *win, int x, int y)
{
    lj_sdl_win *pw = lj_sdl_pw(win);
    if (pw) SDL_SetWindowPosition(pw->sdl, x, y);
}

int lj_platform_window_pos(const lj_window *win, int *x, int *y)
{
    lj_sdl_win *pw = lj_sdl_pw(win);
    return pw && SDL_GetWindowPosition(pw->sdl, x, y) ? 0 : -1;
}

void lj_platform_present(lj_window *win)
{
    lj_sdl_win *pw = lj_sdl_pw(win);
    const lj_window_caption_state *cs;
    lj_rect r[LJ_SDL_MAX_DIRTY];
    int n;

    if (!pw) return;
    lj_window_paint(win);
    cs = lj_window_caption(win);
    if (strcmp(cs->text, pw->title) != 0) {                 /* runtime captions reach the OS title too (native frame, taskbar) */
        char title[512];
        lj_str_copy(pw->title, sizeof pw->title, cs->text);
        lj_cp1252_to_utf8(title, sizeof title, cs->text);
        SDL_SetWindowTitle(pw->sdl, title);
    }
    n = lj_window_dirty(win, r, LJ_SDL_MAX_DIRTY);
    if (n > 0) lj_sdl_blit(pw->sdl, lj_window_surface(win), r, n);   /* n > cap: lj_sdl_blit updates everything */
}

int lj_platform_run(void (*idle)(void *ud), void *ud)
{
    SDL_Event e;
    int i;
    while (!lj_sdl.quit) {
        if (SDL_WaitEventTimeout(&e, 10)) {
            do lj_sdl_dispatch(&e); while (!lj_sdl.quit && SDL_PollEvent(&e));
        }
        if (idle) idle(ud);
        for (i = 0; i < lj_sdl.nwins; i++) lj_platform_present(lj_sdl.wins[i]->win);
    }
    return 0;
}

void lj_platform_quit(void) { lj_sdl.quit = 1; }

void lj_platform_shutdown(void)
{
    if (lj_sdl.timer) SDL_RemoveTimer(lj_sdl.timer);
    while (lj_sdl.nwins > 0) lj_platform_close_window(lj_sdl.wins[lj_sdl.nwins - 1]->win);
    SDL_Quit();
}

/* SDL_Surface (any format) -> new 0x00RRGGBB lj_surface. */
static int lj_sdl_to_surface(SDL_Surface *src, lj_surface *out, const char *what)
{
    SDL_Surface *c = SDL_ConvertSurface(src, SDL_PIXELFORMAT_XRGB8888);
    int y;
    if (!c) { lj_log("SDL_ConvertSurface %s: %s", what, SDL_GetError()); return -1; }
    if (lj_surface_init(out, c->w, c->h) != 0) { SDL_DestroySurface(c); return -1; }
    for (y = 0; y < c->h; y++) memcpy(out->px + (size_t)y * (size_t)c->w, (const char *)c->pixels + (size_t)y * (size_t)c->pitch, (size_t)c->w * 4);
    for (y = 0; y < c->w * c->h; y++) out->px[y] &= 0xFFFFFFu;
    SDL_DestroySurface(c);
    return 0;
}

int lj_platform_load_bmp(const char *path, lj_surface *out)
{
    char u[1024];
    SDL_Surface *bmp;
    int rc;
    if (!path || !out) return -1;
    lj_cp1252_to_utf8(u, sizeof u, path);
    bmp = SDL_LoadBMP(u);
    if (!bmp) { lj_log("SDL_LoadBMP %s: %s", path, SDL_GetError()); return -1; }
    rc = lj_sdl_to_surface(bmp, out, path);
    SDL_DestroySurface(bmp);
    return rc;
}

int lj_platform_clipboard_get(char *dst, size_t cap)
{
    char *t = SDL_GetClipboardText();
    int rc = -1;
    if (!dst || cap == 0) { SDL_free(t); return -1; }
    dst[0] = 0;
    if (t && *t) { lj_utf8_to_cp1252(dst, cap, t); rc = 0; }
    SDL_free(t);
    return rc;
}

int lj_platform_clipboard_set(const char *text)
{
    size_t cap;
    char *u;
    int ok;
    if (!text) return -1;
    cap = strlen(text) * 3 + 1;                          /* a CP1252 byte is at most 3 UTF-8 bytes */
    u = malloc(cap);
    if (!u) lj_fatal("platform: out of memory");
    lj_cp1252_to_utf8(u, cap, text);
    ok = SDL_SetClipboardText(u);
    free(u);
    return ok ? 0 : -1;
}

int lj_platform_pref_path(char *dst, size_t cap)
{
    const char *o = SDL_getenv("LJ4EVR_USERDATA");   /* ME-148: a neutral data folder for captures (docs/running.md); must end in a separator */
    char *p;
    int rc;
    if (o && *o && dst && cap) return lj_str_copy(dst, cap, o);
    p = SDL_GetPrefPath("LJ4EVR", "LJ4EVR");
    if (!dst || cap == 0) { SDL_free(p); return -1; }
    if (!p) { dst[0] = 0; lj_log("SDL_GetPrefPath: %s", SDL_GetError()); return -1; }
    rc = lj_str_copy(dst, cap, p);        /* ME-65: UTF-8 bytes as SDL gave them; the app only concatenates and hands them to fopen (UTF-8 under the manifest), so a non-CP1252 user name still resolves */
    SDL_free(p);
    return rc;
}

void lj_platform_sleep_us(uint64_t us) { SDL_DelayPrecise(us * 1000u); }

/* ---- engine thread (design 3.1) ---------------------------------------------- */

static int SDLCALL lj_sdl_engine_main(void *ud)
{
    uint64_t next = lj_clock_us(), now;
    lj_sacn_cfg sacn;                                                       /* this thread's own copy of the map (ME-23: sender state belongs to the engine thread) */
    const void *newcfg;
    const lj_frame *frame;
    lj_sacn_stats st;
    lj_artnet_stats ast;
    int offline, link, have_sacn = 0;
    (void)ud;
    memset(&sacn, 0, sizeof sacn);
    while (!SDL_GetAtomicInt(&lj_sdl.stop)) {
        now = lj_clock_us();
        if (next > now) lj_platform_sleep_us(next - now);                  /* sleep_until(next) */
        now = lj_clock_us();
        lj_lock(); lj_engine_tick(lj_sdl.engine, now); newcfg = lj_engine_take_sacn_cfg(lj_sdl.engine); offline = lj_engine_is_offline(lj_sdl.engine);
        if (newcfg) memcpy(&sacn, newcfg, sizeof sacn);                     /* ME-69: copied under the lock (Hardware Setup Apply rewrites the record under it) */
        lj_unlock();
        if (newcfg) { lj_sacn_open(&sacn); lj_artnet_open(&sacn); have_sacn = 1; }   /* LJ_CMD_SACN_CFG: close-old then open-new, outside the lock (ME-23); W5: Art-Net from the same record; W5 artnet: the pump runs once a record arrived (each sender no-ops while closed), so Art-Net works with sACN failed */
        if (have_sacn) {                                                    /* design 3.1: set per link unless offline (S9), then one pump */
            frame = lj_engine_frame(lj_sdl.engine);
            if (!offline) for (link = 0; link < LJ_LINKS; link++) {         /* W5 seam 1: the one frame tap, per-link protocol */
                if (sacn.proto[link] == LJ_OUT_ARTNET) lj_artnet_set(link, frame->out[link]);
                else lj_sacn_set(sacn.universe_of_link[link], frame->out[link]);
            }
            lj_sacn_pump(now);
            lj_artnet_pump(now);                                            /* W5: ArtPoll, received packets (Art-Net input to lj_dmxin_post), ArtDmx; no-op while closed */
            lj_sacn_stats_get(&st);
            lj_artnet_stats_get(&ast);                                      /* W5 artnet: pps, nodes, bound reach the view as sacn_pps does */
            lj_lock(); lj_engine_set_sacn_stats(lj_sdl.engine, st.pps, st.last_err); lj_engine_set_artnet_stats(lj_sdl.engine, ast.pps, ast.nodes, ast.bound, ast.last_err); lj_unlock();
        }
        next += lj_sdl.tick_us;                                             /* absolute timeline, never now + tick */
        if (now > next + lj_sdl.tick_us) { lj_lock(); lj_engine_note_overrun(lj_sdl.engine); lj_unlock(); lj_log_overrun(now - next); next = now + lj_sdl.tick_us; }   /* ME-144: the Debug window's Overruns */
    }
    lj_sacn_close();                                                        /* 3 x Stream_Terminated per universe on the owning thread; the app's lj_sacn_close after the join is then a no-op */
    lj_artnet_close();                                                      /* W5: the Art-Net socket belongs to this thread too */
    return 0;
}

int lj_platform_engine_start(struct lj_engine *e, uint32_t tick_us)
{
    if (!e || tick_us == 0 || lj_sdl.thread) return -1;
    lj_sdl.engine = e;
    lj_sdl.tick_us = tick_us;
    SDL_SetAtomicInt(&lj_sdl.stop, 0);
    lj_sdl.thread = SDL_CreateThread(lj_sdl_engine_main, "lj_engine", NULL);
    if (!lj_sdl.thread) { lj_log("SDL_CreateThread: %s", SDL_GetError()); return -1; }
    return 0;
}

void lj_platform_engine_stop(void)
{
    if (!lj_sdl.thread) return;
    SDL_SetAtomicInt(&lj_sdl.stop, 1);
    SDL_WaitThread(lj_sdl.thread, NULL);
    lj_sdl.thread = NULL;
}

/* ---- lj_sdl.h: read-back for --selftest ---------------------------------------- */

int lj_platform_readback(lj_window *win, lj_surface *out)
{
    lj_sdl_win *pw = lj_sdl_pw(win);
    SDL_Surface *ws;
    if (!pw || !out) return -1;
    ws = SDL_GetWindowSurface(pw->sdl);
    if (!ws) { lj_log("SDL_GetWindowSurface: %s", SDL_GetError()); return -1; }
    return lj_sdl_to_surface(ws, out, "window");
}
