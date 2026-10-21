/* LJ4EVR spike "b-lazy": TSequenceForm drawn with SDL3 + SDL_Renderer only.
 *
 * Geometry is TSEQUENCEFORM.dfm.txt (2.209): ClientWidth 466, ClientHeight 163,
 * three TLabels, three read-only clSilver TEdits, Panel1 354x106 with its
 * TBevel, TSpeedButtons, TEdits, TUdButtons, TCheckBoxes.  Chrome is Windows
 * classic: clBtnFace #C0C0C0, highlight #FFFFFF, shadow #808080, dark #000000.
 * Text: SDL3's built-in 8x8 debug font placed on a 13 px line pitch (the real
 * font strategy is the proposal's job; this spike proves window + chrome).
 *
 * The DFM puts Panel1 at (0,0), which would cover the Seq#/Name/Type row; the
 * runtime window shows both, so the panel is drawn at y=40 (noted as a guess).
 * The VCL menu bar is a non-client strip; we draw it as 19 px above the client,
 * so the SDL window is 466 x (19 + 163) and the LJ client is exactly 466 x 163.
 *
 * Runs ~2 s, saves seqwin.bmp from the renderer at ~1 s, exits 0.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

#define CLIENT_W 466
#define CLIENT_H 163
#define MENU_H   19
#define LINE_H   13           /* MS Sans Serif 8 pt text height at 96 dpi */

#define C_FACE   0xC0C0C0     /* clBtnFace == clSilver */
#define C_HILITE 0xFFFFFF
#define C_SHADOW 0x808080
#define C_DARK   0x000000
#define C_TEXT   0x000000

static SDL_Renderer *R;
static int oy;                /* y offset of the LJ client (below the menu bar) */

static void color(Uint32 rgb) { SDL_SetRenderDrawColor(R, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255); }
static void fill(int x, int y, int w, int h, Uint32 c) { SDL_FRect r = { (float)x, (float)(y + oy), (float)w, (float)h }; color(c); SDL_RenderFillRect(R, &r); }
static void hline(int x, int y, int w, Uint32 c) { fill(x, y, w, 1, c); }
static void vline(int x, int y, int h, Uint32 c) { fill(x, y, 1, h, c); }
/* 1-px frame: top/left colour tl, bottom/right colour br */
static void frame(int x, int y, int w, int h, Uint32 tl, Uint32 br) {
    hline(x, y, w, tl); vline(x, y, h, tl);
    hline(x, y + h - 1, w, br); vline(x + w - 1, y, h, br);
}
/* text on a 13 px line: the 8 px glyphs sit 3 px down so caps roughly match MS Sans Serif's cap band */
static void text(int x, int y, const char *s) { color(C_TEXT); SDL_RenderDebugText(R, (float)x, (float)(y + oy + 3), s); }
static int textw(const char *s) { return (int)SDL_strlen(s) * 8; }

/* TBevel, Shape=bsBox Style=bsLowered (1 px) */
static void bevel_box(int x, int y, int w, int h) { frame(x, y, w, h, C_SHADOW, C_HILITE); }

/* TEdit with Ctl3D: sunken 2 px border */
static void edit(int x, int y, int w, int h, Uint32 face, const char *s) {
    fill(x, y, w, h, face);
    frame(x, y, w, h, C_SHADOW, C_HILITE);
    frame(x + 1, y + 1, w - 2, h - 2, C_DARK, C_FACE);
    if (s) text(x + 3, y + (h - LINE_H) / 2, s);
}

/* TSpeedButton, raised, Flat=False: white top/left, black outer bottom/right, grey inner bottom/right */
static void speedbtn(int x, int y, int w, int h, const char *cap) {
    fill(x, y, w, h, C_FACE);
    frame(x, y, w, h, C_HILITE, C_DARK);
    hline(x + 1, y + h - 2, w - 2, C_SHADOW); vline(x + w - 2, y + 1, h - 2, C_SHADOW);
    if (cap) text(x + (w - textw(cap)) / 2, y + (h - LINE_H) / 2, cap);
}

/* TUdButton (custom up/down spinner): two stacked raised buttons */
static void updown(int x, int y, int w, int h) {
    int hh = h / 2;
    speedbtn(x, y, w, hh, NULL);          text(x + (w - 8) / 2, y + hh / 2 - 7, "^");
    speedbtn(x, y + hh, w, h - hh, NULL); text(x + (w - 8) / 2, y + hh + (h - hh) / 2 - 7, "v");
}

/* TCheckBox: 13x13 sunken white box + caption */
static void checkbox(int x, int y, int w, int h, const char *cap, int checked) {
    int by = y + (h - 13) / 2;
    fill(x, by, 13, 13, C_HILITE);
    frame(x, by, 13, 13, C_SHADOW, C_HILITE);
    frame(x + 1, by + 1, 11, 11, C_DARK, C_FACE);
    if (checked) text(x + 3, by - 3, "x");
    (void)w;
    text(x + 17, y + (h - LINE_H) / 2, cap);
}

static void draw(void) {
    /* menu bar (VCL non-client strip), Caption order from the DFM MainMenu1 */
    oy = 0;
    fill(0, 0, CLIENT_W, MENU_H, C_FACE);
    { int x = 7; const char *items[] = { "Disable Fade", "Preferences", "Snapshot", "Help" };
      for (int i = 0; i < 4; i++) { text(x, 3, items[i]); x += textw(items[i]) + 14; } }

    /* LJ client area 466 x 163 */
    oy = MENU_H;
    fill(0, 0, CLIENT_W, CLIENT_H, C_FACE);           /* form Color is clGray in the DFM; the visible area is Panel1/btnface */

    text(7, 2, "Seq#"); text(47, 2, "Sequence Name"); text(215, 2, "Sequence Type");
    edit(7, 15, 33, 21, C_FACE, "0000");              /* ESeqNo   clSilver read-only */
    edit(47, 15, 160, 21, C_FACE, "No Name");         /* ESeqName */
    edit(215, 15, 145, 21, C_FACE, "");               /* ESeqType */

    /* Panel1 354x106, BevelOuter bvNone; drawn at y=40 (DFM says 0,0; see header) */
    { const int px = 0, py = 40;
      fill(px, py, 354, 106, C_FACE);
      bevel_box(px + 1, py + 2, 350, 33);              /* Bevel3 */
      speedbtn(px + 5,   py + 6, 25, 25, "<<");        /* sb3 play reverse */
      speedbtn(px + 31,  py + 6, 25, 25, ">>");        /* sb4 play forward */
      bevel_box(px + 59, py + 6, 73, 25);              /* Elapsed progress bevel */
      speedbtn(px + 133, py + 6, 25, 25, "<");         /* sb1 previous scene */
      speedbtn(px + 159, py + 6, 25, 25, ">");         /* sb2 next scene */
      /* Panel2 54x25, BevelInner+BevelOuter lowered: scene counter */
      fill(px + 186, py + 6, 54, 25, C_FACE);
      frame(px + 186, py + 6, 54, 25, C_SHADOW, C_HILITE);
      frame(px + 187, py + 7, 52, 23, C_SHADOW, C_HILITE);
      text(px + 186 + (54 - textw("1:1")) / 2, py + 6 + (25 - LINE_H) / 2, "1:1");
      speedbtn(px + 243, py + 6, 51, 25, "+");         /* sb7 add scene */
      speedbtn(px + 298, py + 6, 25, 25, "I");         /* sb5 insert scene */
      speedbtn(px + 324, py + 6, 25, 25, "D");         /* sb6 delete scene */

      text(px + 4, py + 41, "Scene Time (sec)");
      edit(px + 95, py + 37, 40, 21, C_HILITE, "1:00");  /* ES (white, editable) */
      updown(px + 137, py + 37, 20, 21);                 /* UdScene */
      text(px + 163, py + 41, "(60.00 bmp)");            /* lBPM; Martin's typo kept (D13) */
      checkbox(px + 239, py + 39, 113, 17, "Global Scene Time", 0);

      text(px + 4, py + 65, "Fade Time (Sec)");
      edit(px + 95, py + 61, 40, 21, C_HILITE, "0:05");  /* EF */
      updown(px + 136, py + 61, 20, 21);                 /* UdFade */
      checkbox(px + 239, py + 58, 113, 17, "Global Fade Time", 0);

      checkbox(px + 4, py + 86, 195, 17, "Synchronize Scene and Fade Times", 0);
      speedbtn(px + 225, py + 78, 25, 25, "N");        /* sb8 new */
      speedbtn(px + 250, py + 78, 25, 25, "S");        /* sb9 save */
      speedbtn(px + 275, py + 78, 25, 25, "L");        /* sb10 load */
      speedbtn(px + 300, py + 78, 25, 25, "+C");       /* sb11 add to cue */
      speedbtn(px + 326, py + 78, 25, 25, "B");        /* seqBlind */
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_Window *W = SDL_CreateWindow("Sequence", CLIENT_W, MENU_H + CLIENT_H, SDL_WINDOW_ALWAYS_ON_TOP);
    if (!W) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_SetWindowPosition(W, 200, 200);
    R = SDL_CreateRenderer(W, NULL);
    if (!R) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }
    SDL_SetRenderVSync(R, 1);
    { int px, py; SDL_GetWindowPosition(W, &px, &py); printf("POS %d %d\n", px, py); fflush(stdout); }

    int saved = 0;
    Uint64 t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 2000) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) goto done;
        draw();
        if (!saved && SDL_GetTicks() - t0 > 1000) {
            SDL_Surface *s = SDL_RenderReadPixels(R, NULL);
            if (s) { SDL_SaveBMP(s, "seqwin.bmp"); SDL_DestroySurface(s); }
            saved = 1;
        }
        SDL_RenderPresent(R);
    }
done:
    { int w, h; SDL_GetWindowSizeInPixels(W, &w, &h); printf("window pixels %dx%d (LJ client %dx%d + %d px menu bar)\n", w, h, CLIENT_W, CLIENT_H, MENU_H); }
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(W);
    SDL_Quit();
    return 0;
}
