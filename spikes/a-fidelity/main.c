/* LJ4EVR spike a-fidelity: TSequenceForm from the 2.209 DFM, drawn Windows-classic by hand.
 *
 * Stack: SDL3 (window + event pump + surface blit) and nothing else; every pixel below is ours.
 * Geometry: D:/lj4evr/reference/installer/exe_resources/dfm_text/TSEQUENCEFORM.dfm.txt
 *   ClientWidth 466, ClientHeight 163, Color clGray, MS Sans Serif -11, TextHeight 13, menu bar.
 * Font: on Windows the glyphs come from the system raster font "MS Sans Serif" (sserife.fon) via GDI,
 *   rendered once per string into a scratch DIB and copied as 1-bit pixels; the program prints the
 *   GDI metrics (tmHeight must be 13) and checks the DFM label widths, which Delphi computed with the
 *   same font (AutoSize labels), so a match proves metric identity.
 * Runs 2 s, exits 0.  `--explode` moves Panel1 down 40 px so the labels/edits it covers are visible.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Delphi/Windows-classic colours (RGB). */
#define C_BTNFACE  0xC0C0C0u   /* clBtnFace, clSilver */
#define C_HILIGHT  0xFFFFFFu   /* clBtnHighlight, clWindow */
#define C_SHADOW   0x808080u   /* clBtnShadow, clGray */
#define C_BLACK    0x000000u   /* cl3DDkShadow, clWindowText */

#define MENU_H 19              /* SM_CYMENU on classic Windows at 96 dpi; Delphi ClientHeight excludes it */

static SDL_Surface *g_surf;
static int g_ox = 0, g_oy = MENU_H;   /* client-area origin inside the SDL surface */

static void fill(int x, int y, int w, int h, unsigned rgb) {
    SDL_Rect r = { g_ox + x, g_oy + y, w, h };
    SDL_FillSurfaceRect(g_surf, &r, SDL_MapSurfaceRGB(g_surf, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb));
}
static void hline(int x, int y, int w, unsigned c) { fill(x, y, w, 1, c); }
static void vline(int x, int y, int h, unsigned c) { fill(x, y, 1, h, c); }

/* 1-px bevel: top/left = tl colour, bottom/right = br colour (TPanel bvRaised/bvLowered, TBevel box). */
static void bevel1(int x, int y, int w, int h, unsigned tl, unsigned br) {
    hline(x, y, w - 1, tl); vline(x, y, h - 1, tl);
    hline(x, y + h - 1, w, br); vline(x + w - 1, y, h, br);
}
/* Classic raised push button: white outer top/left, black outer bottom/right, grey inner bottom/right. */
static void button_raised(int x, int y, int w, int h) {
    fill(x, y, w, h, C_BTNFACE);
    bevel1(x, y, w, h, C_HILIGHT, C_BLACK);
    hline(x + 1, y + h - 2, w - 2, C_SHADOW); vline(x + w - 2, y + 1, h - 2, C_SHADOW);
}
/* Classic sunken edit/checkbox frame: grey+black outer/inner top-left, white+btnface bottom-right. */
static void sunken2(int x, int y, int w, int h, unsigned interior) {
    fill(x, y, w, h, interior);
    bevel1(x, y, w, h, C_SHADOW, C_HILIGHT);
    bevel1(x + 1, y + 1, w - 2, h - 2, C_BLACK, C_BTNFACE);
}

/* ---------------- text: system raster font via GDI (Windows); stub elsewhere ---------------- */
#ifdef _WIN32
static HDC g_dc; static HBITMAP g_dib; static unsigned *g_bits; enum { DIB_W = 512, DIB_H = 16 };
static int font_init(void) {
    g_dc = CreateCompatibleDC(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = DIB_W; bi.bmiHeader.biHeight = -DIB_H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    g_dib = CreateDIBSection(g_dc, &bi, DIB_RGB_COLORS, (void **)&g_bits, NULL, 0);
    SelectObject(g_dc, g_dib);
    /* lfHeight -11 = 8 pt at 96 dpi = the DFM's Font.Height; the raster face has a 13 px cell (11 + 2 leading). */
    HFONT f = CreateFontA(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
                          NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
    SelectObject(g_dc, f);
    SetBkMode(g_dc, OPAQUE); SetBkColor(g_dc, RGB(255, 255, 255)); SetTextColor(g_dc, RGB(0, 0, 0));
    TEXTMETRICA tm; GetTextMetricsA(g_dc, &tm);
    char face[64]; GetTextFaceA(g_dc, sizeof face, face);
    printf("font: face='%s' raster=%s tmHeight=%d tmAscent=%d tmDescent=%d tmInternalLeading=%d tmAveCharWidth=%d\n",
           face, (tm.tmPitchAndFamily & TMPF_TRUETYPE) ? "no(TrueType!)" : "yes", (int)tm.tmHeight, (int)tm.tmAscent,
           (int)tm.tmDescent, (int)tm.tmInternalLeading, (int)tm.tmAveCharWidth);
    return tm.tmHeight;
}
static int text_width(const char *s) { SIZE sz; GetTextExtentPoint32A(g_dc, s, (int)strlen(s), &sz); return (int)sz.cx; }
/* Draw s with its 13-px cell top at (x,y) in client coords; '&' underlines the next char (Delphi accelerator). */
static void text_draw(int x, int y, const char *s, unsigned rgb) {
    char plain[256]; int n = 0, ul_at = -1;
    for (const char *p = s; *p && n < 255; p++) { if (*p == '&' && p[1] && p[1] != '&') { ul_at = n; continue; } plain[n++] = *p; }
    plain[n] = 0;
    SIZE sz; GetTextExtentPoint32A(g_dc, plain, n, &sz);
    RECT rc = { 0, 0, DIB_W, DIB_H }; FillRect(g_dc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    TextOutA(g_dc, 0, 0, plain, n); GdiFlush();
    for (int yy = 0; yy < sz.cy && yy < DIB_H; yy++)
        for (int xx = 0; xx < sz.cx && xx < DIB_W; xx++)
            if ((g_bits[yy * DIB_W + xx] & 0xFFFFFF) == 0)
                SDL_WriteSurfacePixel(g_surf, g_ox + x + xx, g_oy + y + yy, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255);
    if (ul_at >= 0) {   /* underline the accelerator char at the baseline + 1 (ascent 11 -> row 12) */
        SIZE a, b; GetTextExtentPoint32A(g_dc, plain, ul_at, &a); GetTextExtentPoint32A(g_dc, plain, ul_at + 1, &b);
        hline(x + a.cx, y + 12, b.cx - a.cx, rgb);
    }
}
#else
/* ponytail: mac path = embedded Wine ms_sans_serif bitmap strikes (LGPL 2.1+); not built in this spike. */
static int font_init(void) { printf("font: no backend on this OS in the spike\n"); return 0; }
static int text_width(const char *s) { return 6 * (int)strlen(s); }
static void text_draw(int x, int y, const char *s, unsigned rgb) { (void)x; (void)y; (void)s; (void)rgb; }
#endif

/* ---------------- widgets as Delphi draws them ---------------- */
static void label(int x, int y, const char *cap) { text_draw(x, y, cap, C_BLACK); }
static void edit(int x, int y, int w, int h, unsigned interior, const char *txt) {
    sunken2(x, y, w, h, interior);
    if (txt && *txt) text_draw(x + 3, y + (h - 13) / 2, txt, C_BLACK);
}
static void speedbutton(int x, int y, int w, int h) { button_raised(x, y, w, h); }   /* glyphs are Martin art: redrawn later (D2) */
static void tri(int cx, int cy, int up) {            /* 5-px-wide arrow head for the up/down buttons */
    for (int i = 0; i < 3; i++) hline(cx - i, up ? cy + i : cy - i, 2 * i + 1, C_BLACK);
}
static void updown(int x, int y, int w, int h) {    /* TUdButton 20x21: two stacked raised buttons */
    int h1 = h / 2;
    button_raised(x, y, w, h1); button_raised(x, y + h1, w, h - h1);
    tri(x + w / 2, y + h1 / 2 - 1, 1); tri(x + w / 2, y + h1 + (h - h1) / 2 + 1, 0);
}
static void checkbox(int x, int y, int w, int h, const char *cap) {
    (void)w;
    sunken2(x, y + (h - 13) / 2, 13, 13, C_HILIGHT);
    text_draw(x + 17, y + (h - 13) / 2, cap, C_BLACK);
}
static void menubar(const char **items, int n) {
    int save_oy = g_oy; g_oy = 0;
    fill(0, 0, g_surf->w, MENU_H, C_BTNFACE);
    int x = 7;                                       /* classic menu item padding: approximation, not from a DFM */
    for (int i = 0; i < n; i++) { text_draw(x, 3, items[i], C_BLACK); x += text_width(items[i]) + 14; }
    g_oy = save_oy;
}

static void draw_sequence_form(int panel_top) {
    /* form: Color = clGray */
    fill(0, 0, 466, 163 + (panel_top ? panel_top : 0), C_SHADOW);
    /* TLabel x3 (AutoSize widths 26/80/76 in the DFM) and TEdit x3 (clSilver, ReadOnly) */
    label(7, 2, "Seq#"); label(47, 2, "Sequence Name"); label(215, 2, "Sequence Type");
    edit(7, 15, 33, 21, C_BTNFACE, NULL); edit(47, 15, 160, 21, C_BTNFACE, NULL); edit(215, 15, 145, 21, C_BTNFACE, NULL);
    /* Panel1 (0,0) 354x106 BevelOuter=bvNone: declared after the edits, so it covers them (DFM z-order) */
    int py = panel_top;
    fill(0, py, 354, 106, C_BTNFACE);
    bevel1(1, py + 2, 350, 33, C_SHADOW, C_HILIGHT);                 /* Bevel3, bsLowered box */
    speedbutton(5, py + 6, 25, 25); speedbutton(31, py + 6, 25, 25);  /* sb3 sb4 playback */
    bevel1(59, py + 6, 73, 25, C_SHADOW, C_HILIGHT);                 /* Elapsed bevel */
    speedbutton(133, py + 6, 25, 25); speedbutton(159, py + 6, 25, 25); /* sb1 sb2 prev/next */
    fill(186, py + 6, 54, 25, C_BTNFACE);                            /* Panel2 bvLowered inner+outer */
    bevel1(186, py + 6, 54, 25, C_SHADOW, C_HILIGHT); bevel1(187, py + 7, 52, 23, C_SHADOW, C_HILIGHT);
    speedbutton(243, py + 6, 51, 25); speedbutton(298, py + 6, 25, 25); speedbutton(324, py + 6, 25, 25); /* sb7 sb5 sb6 */
    label(4, py + 41, "Scene Time (sec)"); edit(95, py + 37, 40, 21, C_HILIGHT, "88:88"); updown(137, py + 37, 20, 21);
    label(163, py + 41, "XXXX.X BPM");
    label(4, py + 65, "Fade Time (Sec)");  edit(95, py + 61, 40, 21, C_HILIGHT, "88:88"); updown(136, py + 61, 20, 21);
    checkbox(239, py + 39, 113, 17, "Global &Scene Time");
    checkbox(239, py + 58, 113, 17, "Global &Fade Time");
    checkbox(4, py + 86, 195, 17, "S&ynchronize Scene and Fade Times");
    speedbutton(225, py + 78, 25, 25); speedbutton(250, py + 78, 25, 25); speedbutton(275, py + 78, 25, 25);
    speedbutton(300, py + 78, 25, 25); speedbutton(326, py + 78, 25, 25); /* sb8-sb11, seqBlind */
    static const char *menu[] = { "Disable Fade", "&Preferences", "Snapshot", "&Help" };
    menubar(menu, 4);
}

int main(int argc, char **argv) {
    int explode = 0;
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--explode") == 0) explode = 1;
    int panel_top = explode ? 40 : 0;
    int cw = 466, ch = 163 + panel_top;

    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("Sequence", cw, ch + MENU_H, 0);   /* bsSingle: not resizable */
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_SetWindowPosition(win, 100, 100);
    g_surf = SDL_GetWindowSurface(win);
    int tmh = font_init();
    printf("window client %dx%d (+%d menu bar), surface %dx%d\n", cw, ch, MENU_H, g_surf->w, g_surf->h);

    /* DFM AutoSize label widths vs this font: identical numbers mean identical metrics. */
    static const struct { const char *s; int dfm; } checks[] = {
        { "Seq#", 26 }, { "Sequence Name", 80 }, { "Sequence Type", 76 },
        { "Scene Time (sec)", 83 }, { "Fade Time (Sec)", 78 }, { "XXXX.X BPM", 64 } };
    int bad = 0;
    for (size_t i = 0; i < sizeof checks / sizeof checks[0]; i++) {
        int w = text_width(checks[i].s);
        printf("width '%s': measured %d, DFM %d %s\n", checks[i].s, w, checks[i].dfm, w == checks[i].dfm ? "OK" : "MISMATCH");
        bad += w != checks[i].dfm;
    }
    printf("text height 13 px: %s; label widths: %d mismatches\n", tmh == 13 ? "OK" : "FAIL", bad);

    draw_sequence_form(panel_top);
    SDL_UpdateWindowSurface(win);
    for (int i = 1; i + 1 < argc; i++)                      /* --save <file.bmp>: dump the drawn surface */
        if (strcmp(argv[i], "--save") == 0) printf("saved %s: %s\n", argv[i + 1], SDL_SaveBMP(g_surf, argv[i + 1]) ? "ok" : SDL_GetError());

    Uint64 t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 2000) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_WINDOW_EXPOSED) { g_surf = SDL_GetWindowSurface(win); draw_sequence_form(panel_top); SDL_UpdateWindowSurface(win); }
        }
        SDL_Delay(10);
    }
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
