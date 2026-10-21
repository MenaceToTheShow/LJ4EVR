/* LJ4EVR spike (architect c-engineering): one SDL3 window drawn as the
 * LightJockey 2 Sequence control, Windows-classic chrome painted by the clone,
 * 13 px text rows. Runs for 2 s and exits 0; writes a BMP of its own back
 * buffer when given --shot <path>.
 *
 * Stack under test: C11 + SDL3 (window, input, 2D renderer) + a widget table
 * generated from the DFM (seqform_dfm.h) + an in-repo 1-bit font (font13.h).
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string.h>
#include "font13.h"
#include "seqform_dfm.h"

/* Windows-classic system colours (Delphi names) */
#define CLR(r, g, b) { (Uint8)(r), (Uint8)(g), (Uint8)(b), 255 }
static const SDL_Color clBtnFace      = CLR(0xC0, 0xC0, 0xC0);
static const SDL_Color clBtnHighlight = CLR(0xFF, 0xFF, 0xFF);
static const SDL_Color clBtnShadow    = CLR(0x80, 0x80, 0x80);
static const SDL_Color clWindowFrame  = CLR(0x00, 0x00, 0x00);
static const SDL_Color clWindow       = CLR(0xFF, 0xFF, 0xFF);
static const SDL_Color clWindowText   = CLR(0x00, 0x00, 0x00);
static const SDL_Color clSilver       = CLR(0xC0, 0xC0, 0xC0);

static SDL_Renderer *R;
static SDL_Texture *font_tex;
#define FONT_CELL_W 16
#define MENUBAR_H 19   /* classic menu bar height at 96 dpi */

static void set_color(SDL_Color c) { SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a); }

static void fill(int x, int y, int w, int h, SDL_Color c)
{
    SDL_FRect r = { (float)x, (float)y, (float)w, (float)h };
    set_color(c);
    SDL_RenderFillRect(R, &r);
}

/* one-pixel frame: top/left in tl, bottom/right in br (the DrawEdge building block) */
static void frame(int x, int y, int w, int h, SDL_Color tl, SDL_Color br)
{
    fill(x, y, w - 1, 1, tl);          /* top */
    fill(x, y, 1, h - 1, tl);          /* left */
    fill(x, y + h - 1, w, 1, br);      /* bottom */
    fill(x + w - 1, y, 1, h, br);      /* right */
}

static void bevel_raised_button(int x, int y, int w, int h)   /* TSpeedButton, up state */
{
    fill(x, y, w, h, clBtnFace);
    frame(x, y, w, h, clBtnHighlight, clWindowFrame);
    frame(x + 1, y + 1, w - 2, h - 2, clBtnFace, clBtnShadow);
}

static void bevel_sunken_edit(int x, int y, int w, int h, SDL_Color bg)  /* TEdit, ctl3d */
{
    fill(x, y, w, h, bg);
    frame(x, y, w, h, clBtnShadow, clBtnHighlight);
    frame(x + 1, y + 1, w - 2, h - 2, clWindowFrame, clBtnFace);
}

static void bevel_lowered_1px(int x, int y, int w, int h)   /* TBevel bsBox lowered */
{
    frame(x, y, w, h, clBtnShadow, clBtnHighlight);
}

static int text_width(const char *s)
{
    int w = 0;
    for (; *s; s++) {
        unsigned c = (unsigned char)*s;
        if (c == '&') continue;                       /* VCL accelerator marker */
        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT) c = '?';
        w += FONT[c - FONT_FIRST].adv;
    }
    return w;
}

/* draws one 13 px text row with its top at y; an ampersand underlines the next glyph as the VCL does */
static void text(int x, int y, const char *s, SDL_Color c)
{
    int underline_next = 0;
    SDL_SetTextureColorMod(font_tex, c.r, c.g, c.b);
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        int gi;
        SDL_FRect src, dst;
        if (ch == '&') { underline_next = 1; continue; }
        if (ch < FONT_FIRST || ch >= FONT_FIRST + FONT_COUNT) ch = '?';
        gi = (int)ch - FONT_FIRST;
        src.x = (float)(gi * FONT_CELL_W); src.y = 0; src.w = FONT_CELL_W; src.h = FONT_LINE_H;
        dst.x = (float)x; dst.y = (float)y; dst.w = FONT_CELL_W; dst.h = FONT_LINE_H;
        SDL_RenderTexture(R, font_tex, &src, &dst);
        if (underline_next) { fill(x, y + FONT_LINE_H - 2, FONT[gi].adv, 1, c); underline_next = 0; }
        x += FONT[gi].adv;
    }
}

static void checkbox(int x, int y, int h, const char *caption)
{
    int by = y + (h - 13) / 2;
    bevel_sunken_edit(x, by, 13, 13, clWindow);
    text(x + 17, y + (h - FONT_LINE_H) / 2, caption, clWindowText);
}

static void udbutton(int x, int y, int w, int h)   /* TUdButton: two stacked raised halves */
{
    int h1 = h / 2, cx = x + w / 2, i;
    bevel_raised_button(x, y, w, h1);
    bevel_raised_button(x, y + h1, w, h - h1);
    for (i = 0; i < 3; i++) {                     /* up and down triangles */
        fill(cx - i, y + h1 / 2 - 1 + i, 1 + 2 * i, 1, clWindowText);
        fill(cx - i, y + h1 + h1 / 2 + 1 - i, 1 + 2 * i, 1, clWindowText);
    }
}

static void menubar(void)
{
    int x = 7, i;
    fill(0, 0, SEQFORM_CLIENT_WIDTH, MENUBAR_H, clBtnFace);
    for (i = 0; i < SEQFORM_MENU_COUNT; i++) {
        text(x, (MENUBAR_H - FONT_LINE_H) / 2, SEQFORM_MENU[i], clWindowText);
        x += text_width(SEQFORM_MENU[i]) + 14;
    }
}

static void draw_form(void)
{
    int i;
    fill(0, 0, SEQFORM_CLIENT_WIDTH, SEQFORM_CLIENT_HEIGHT + MENUBAR_H, clBtnFace);
    menubar();
    for (i = 0; i < SEQFORM_COUNT; i++) {
        const struct widget *w = &SEQFORM[i];
        int x = w->x, y = w->y + MENUBAR_H;
        if (w->parent >= 0) {
            x += SEQFORM[w->parent].x;
            y += SEQFORM_PANEL1_TOP_SPIKE;          /* see seqform_dfm.h */
        }
        switch (w->kind) {
        case W_LABEL:   text(x, y, w->caption, clWindowText); break;
        case W_EDIT: {
            SDL_Color bg = (w->parent < 0) ? clSilver : clWindow;   /* ESeq* are clSilver read-only */
            bevel_sunken_edit(x, y, w->w, w->h, bg);
            if (w->caption && *w->caption) text(x + 3, y + (w->h - FONT_LINE_H) / 2, w->caption, clWindowText);
            break; }
        case W_PANEL:   fill(x, y + SEQFORM_PANEL1_TOP_SPIKE, w->w, w->h, clBtnFace); break;
        case W_BEVEL:   bevel_lowered_1px(x, y, w->w, w->h); break;
        case W_SPEEDBTN: bevel_raised_button(x, y, w->w, w->h); break;
        case W_PANEL_LOWERED2:
            bevel_lowered_1px(x, y, w->w, w->h);
            bevel_lowered_1px(x + 1, y + 1, w->w - 2, w->h - 2);
            break;
        case W_CHECKBOX: checkbox(x, y, w->h, w->caption); break;
        case W_UDBUTTON: udbutton(x, y, w->w, w->h); break;
        case W_MENUBAR: break;
        }
    }
}

static SDL_Texture *build_font_texture(void)
{
    SDL_Surface *s = SDL_CreateSurface(FONT_COUNT * FONT_CELL_W, FONT_LINE_H, SDL_PIXELFORMAT_RGBA32);
    SDL_Texture *t;
    Uint32 *px;
    int pitch4, g, x, y;
    if (!s) return NULL;
    px = (Uint32 *)s->pixels;
    pitch4 = s->pitch / 4;
    for (g = 0; g < FONT_COUNT; g++)
        for (y = 0; y < FONT_LINE_H; y++)
            for (x = 0; x < FONT_CELL_W; x++)
                px[y * pitch4 + g * FONT_CELL_W + x] = ((FONT[g].rows[y] >> x) & 1) ? 0xFFFFFFFFu : 0u;
    t = SDL_CreateTextureFromSurface(R, s);
    SDL_DestroySurface(s);
    if (t) { SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST); SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); }
    return t;
}

int main(int argc, char **argv)
{
    const char *shot = NULL;
    Uint32 run_ms = 2000;
    Uint64 t0;
    int shot_done = 0, i;
    SDL_Window *win;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--ms") && i + 1 < argc) run_ms = (Uint32)SDL_atoi(argv[++i]);
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }

    /* client area = DFM ClientWidth x ClientHeight plus the clone-drawn menu bar */
    win = SDL_CreateWindow(SEQFORM_CAPTION, SEQFORM_CLIENT_WIDTH, SEQFORM_CLIENT_HEIGHT + MENUBAR_H, 0);
    if (!win) { SDL_Log("SDL_CreateWindow: %s", SDL_GetError()); return 1; }
    R = SDL_CreateRenderer(win, NULL);
    if (!R) { SDL_Log("SDL_CreateRenderer: %s", SDL_GetError()); return 1; }
    SDL_SetRenderVSync(R, 1);
    font_tex = build_font_texture();
    if (!font_tex) { SDL_Log("font texture: %s", SDL_GetError()); return 1; }

    t0 = SDL_GetTicks();
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) run_ms = 0;
        draw_form();
        if (shot && !shot_done && SDL_GetTicks() - t0 > 300) {
            SDL_Surface *s = SDL_RenderReadPixels(R, NULL);
            if (s) { SDL_SaveBMP(s, shot); SDL_DestroySurface(s); }
            shot_done = 1;
        }
        SDL_RenderPresent(R);
        if (SDL_GetTicks() - t0 >= run_ms) break;
    }
    SDL_DestroyTexture(font_tex);
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
