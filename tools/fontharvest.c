/* fontharvest.c - Windows-only GDI developer tool (design 7.4 step 2, ME-18). Never shipped,
 * never in CI, not in the CMake build. For every (face, height, style) row of
 * tests/fonts/font_census.tsv it creates the font with the DFM height at 96 dpi and the
 * ANSI charset (so bytes 32..255 are CP1252), then writes tests/fonts/advances_<name>.tsv:
 *   line 1  height ascent descent internal_leading face dfm_height style overhang   (field names)
 *   line 2  the values, tab separated
 *   lines 3..  <code>\t<advance>  for codes 32..255
 * overhang = tmOverhang: GDI synthesizes bold for a raster face by overstriking 1 px right;
 * every advance grows by 1 AND DrawText (what Delphi's AutoSize measures) adds the overhang
 * once more per string. A label width is sum(advances) + overhang; GetTextExtentPoint32 does
 * not add it, which is why the check below uses DrawText DT_CALCRECT.
 * NUMBERS ONLY: no glyph bitmap leaves this tool (that would redistribute Microsoft's font).
 * <name> = sans|arial|arialblack|courier + tmHeight + "" | "b" | "i" | "bi" (style & 3;
 * underline and strikeout do not change advances). Two census rows that land on the same
 * strike (Courier -11 and -13; sans -11 bold and bold+underline) share one file.
 * Verifies tmHeight 13 for MS Sans Serif -11 and that the six TSEQUENCEFORM label widths
 * 26/80/76/83/78/64 are sums of advances (and equal GetTextExtentPoint32).
 *
 * build + run (vcvars64):  cl /nologo /W4 /WX /permissive- /utf-8 /std:c11 /D_CRT_SECURE_NO_WARNINGS tools\fontharvest.c gdi32.lib
 *                          fontharvest.exe tests\fonts\font_census.tsv tests\fonts */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static char g_done[64][32];
static int g_ndone;

static const char *facekey(const char *face)
{
    if (!strcmp(face, "MS Sans Serif")) return "sans";
    if (!strcmp(face, "Arial")) return "arial";
    if (!strcmp(face, "Arial Black")) return "arialblack";
    if (!strcmp(face, "Courier")) return "courier";
    return "unknown";
}

static int text_width(const int *adv, const char *s)       /* sum of advances, '&' as the VCL: "&x" = x, "&&" = '&' */
{
    int w = 0;
    for (; *s; s++) {
        if (*s == '&' && s[1] && s[1] != '&') continue;
        if (*s == '&' && s[1] == '&') s++;
        w += adv[(unsigned char)*s - 32];
    }
    return w;
}

static int harvest(const char *face, int dfm_height, int style, const char *outdir)
{
    HDC dc = CreateCompatibleDC(NULL);
    HFONT f = CreateFontA(dfm_height, 0, 0, 0, (style & 1) ? FW_BOLD : FW_NORMAL, (style & 2) ? 1 : 0, 0, 0,
                          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, face);
    TEXTMETRICA tm;
    char got[64], path[512], name[32];
    int adv[224], c, i, abc_mismatch = 0, fresh = 1;
    FILE *out;
    if (!dc || !f) { printf("FAIL create %s %d\n", face, dfm_height); return -1; }
    SelectObject(dc, f);
    GetTextMetricsA(dc, &tm);
    GetTextFaceA(dc, sizeof got, got);
    if (strcmp(got, face) != 0) { printf("FAIL substituted: wanted '%s' got '%s'\n", face, got); g_fail++; }
    if (!GetCharWidth32A(dc, 32, 255, adv)) { printf("FAIL GetCharWidth32 %s\n", face); return -1; }
    if (tm.tmPitchAndFamily & TMPF_TRUETYPE) {               /* cross-check: advance == A+B+C */
        ABC abc[224];
        if (GetCharABCWidthsA(dc, 32, 255, abc))
            for (c = 0; c < 224; c++) if (abc[c].abcA + (int)abc[c].abcB + abc[c].abcC != adv[c]) abc_mismatch++;
    }
    snprintf(name, sizeof name, "%s%d%s%s", facekey(face), (int)tm.tmHeight, (style & 1) ? "b" : "", (style & 2) ? "i" : "");
    for (i = 0; i < g_ndone; i++) if (!strcmp(g_done[i], name)) fresh = 0;
    printf("%-14s %-14s %4d style %d -> tmHeight %2d ascent %2d descent %d leading %d overhang %d %s abc_mismatch %d%s\n",
           name, face, dfm_height, style, (int)tm.tmHeight, (int)tm.tmAscent, (int)tm.tmDescent,
           (int)tm.tmInternalLeading, (int)tm.tmOverhang, (tm.tmPitchAndFamily & TMPF_TRUETYPE) ? "truetype" : "raster",
           abc_mismatch, fresh ? "" : " (same strike, file kept)");
    if (fresh && g_ndone < 64) {
        snprintf(g_done[g_ndone++], sizeof g_done[0], "%s", name);
        snprintf(path, sizeof path, "%s/advances_%s.tsv", outdir, name);
        out = fopen(path, "wb");
        if (!out) { printf("FAIL open %s\n", path); return -1; }
        fprintf(out, "height\tascent\tdescent\tinternal_leading\tface\tdfm_height\tstyle\toverhang\n");
        fprintf(out, "%d\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n", (int)tm.tmHeight, (int)tm.tmAscent, (int)tm.tmDescent,
                (int)tm.tmInternalLeading, face, dfm_height, style & 3, (int)tm.tmOverhang);
        for (c = 0; c < 224; c++) fprintf(out, "%d\t%d\n", c + 32, adv[c]);
        fclose(out);
    }
    if (!strcmp(face, "MS Sans Serif") && dfm_height == -11 && style == 0) {
        static const char *cap[6] = { "Seq#", "Sequence Name", "Sequence Type", "Scene Time (sec)", "Fade Time (Sec)", "XXXX.X BPM" };
        static const int want[6] = { 26, 80, 76, 83, 78, 64 };
        int bad = 0;
        if (tm.tmHeight != 13) { printf("FAIL tmHeight %d != 13\n", (int)tm.tmHeight); bad++; }
        for (i = 0; i < 6; i++) {
            SIZE sz;
            int w = text_width(adv, cap[i]);
            GetTextExtentPoint32A(dc, cap[i], (int)strlen(cap[i]), &sz);
            if (w != want[i] || sz.cx != want[i]) { printf("FAIL '%s' sum %d extent %ld want %d\n", cap[i], w, sz.cx, want[i]); bad++; }
        }
        printf("  tmHeight 13 and the TSEQUENCEFORM six widths 26/80/76/83/78/64: %s\n", bad ? "MISMATCH" : "ok");
        g_fail += bad;
    }
    {                                                          /* the AutoSize model: DrawText width == sum(advances) + overhang */
        RECT rc = { 0, 0, 0, 0 };
        DrawTextA(dc, "Sequence Name", 13, &rc, DT_CALCRECT | DT_NOPREFIX);
        if (rc.right != text_width(adv, "Sequence Name") + tm.tmOverhang) {
            printf("FAIL DrawText %ld != sum %d + overhang %d\n", rc.right, text_width(adv, "Sequence Name"), (int)tm.tmOverhang); g_fail++;
        }
    }
    DeleteObject(f);
    DeleteDC(dc);
    return 0;
}

int main(int argc, char **argv)
{
    FILE *in;
    char line[1024];
    int n = 0;
    if (argc != 3) { fprintf(stderr, "usage: fontharvest <font_census.tsv> <outdir>\n"); return 2; }
    in = fopen(argv[1], "rb");
    if (!in) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    if (!fgets(line, sizeof line, in)) return 2;              /* header */
    while (fgets(line, sizeof line, in)) {
        char *face = strtok(line, "\t"), *h = strtok(NULL, "\t"), *st = strtok(NULL, "\t");
        if (!face || !h || !st) continue;
        if (harvest(face, atoi(h), atoi(st), argv[2]) != 0) g_fail++;
        n++;
    }
    fclose(in);
    printf("%d census rows, %d distinct advance files, %d failures\n", n, g_ndone, g_fail);
    return g_fail ? 1 : 0;
}
