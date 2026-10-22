/* lj_str.c - bounded CP1252 strings and the UTF-8 boundary conversions (design 2.1). */
#include <stdio.h>
#include <string.h>
#include "core/lj_str.h"

/* Windows-1252 bytes 0x80..0xFF -> Unicode. The five bytes Microsoft leaves
 * undefined (81 8D 8F 90 9D) map to the C1 controls of the same value, as
 * MultiByteToWideChar does, so every byte round-trips. */
static const uint16_t lj_cp1252_hi[128] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

int lj_str_copy(char *dst, size_t cap, const char *src)
{
    size_t n;
    if (cap == 0) return -1;
    n = strlen(src);
    if (n >= cap) { memcpy(dst, src, cap - 1); dst[cap - 1] = 0; return -1; }
    memcpy(dst, src, n + 1);
    return 0;
}

int lj_cp1252_to_utf8(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    const unsigned char *s = (const unsigned char *)src;
    if (cap == 0) return -1;
    for (; *s; s++) {
        uint32_t cp = *s < 0x80 ? *s : lj_cp1252_hi[*s - 0x80];
        size_t need = cp < 0x80 ? 1 : cp < 0x800 ? 2 : 3;
        if (o + need >= cap) break;
        if (need == 1) dst[o++] = (char)cp;
        else if (need == 2) { dst[o++] = (char)(0xC0 | (cp >> 6)); dst[o++] = (char)(0x80 | (cp & 0x3F)); }
        else { dst[o++] = (char)(0xE0 | (cp >> 12)); dst[o++] = (char)(0x80 | ((cp >> 6) & 0x3F)); dst[o++] = (char)(0x80 | (cp & 0x3F)); }
    }
    dst[o] = 0;
    return *s ? -1 : 0;
}

/* Decodes one well-formed sequence at s into *cp; returns its length, 0 on malformed. */
static int lj_utf8_decode(const unsigned char *s, uint32_t *cp)
{
    int n, i;
    uint32_t c = s[0], min;
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xC2 && c <= 0xDF) { n = 2; c &= 0x1F; min = 0x80; }
    else if (c >= 0xE0 && c <= 0xEF) { n = 3; c &= 0x0F; min = 0x800; }
    else if (c >= 0xF0 && c <= 0xF4) { n = 4; c &= 0x07; min = 0x10000; }
    else return 0;
    for (i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) return 0;
        c = (c << 6) | (s[i] & 0x3F);
    }
    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0;   /* overlong, out of range, surrogate */
    *cp = c;
    return n;
}

int lj_utf8_to_cp1252(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    int bad = 0;
    const unsigned char *s = (const unsigned char *)src;
    if (cap == 0) return -1;
    while (*s) {
        uint32_t cp;
        int n = lj_utf8_decode(s, &cp), k;
        char out = '?';
        if (n == 0) { bad = 1; break; }
        s += n;
        if (cp < 0x80) out = (char)cp;
        else for (k = 0; k < 128; k++) if (lj_cp1252_hi[k] == cp) { out = (char)(0x80 + k); break; }
        if (o + 1 >= cap) { bad = 1; break; }
        dst[o++] = out;
    }
    dst[o] = 0;
    return bad ? -1 : 0;
}

int lj_fmt_time(char *dst, size_t cap, uint32_t ms)
{
    int n = snprintf(dst, cap, "%u:%02u", (unsigned)(ms / 1000), (unsigned)(ms % 1000 / 10));
    return n < 0 || (size_t)n >= cap ? -1 : 0;
}

/* RTF -> plain text for the user's profile notes (design 5.3, LORD_FIDELITY-17). Not a parser: groups that start with
 * \* or a header destination (fonttbl colortbl stylesheet info pict header footer) are dropped whole, every other
 * control word is dropped, \par / \line = newline, \tab = tab, \'hh = that byte, backslash-escaped \ { } = the
 * literal; CR / LF in the source are formatting only. rtf must be NUL-terminated (a .GS2 notes field need not be:
 * copy it first). */
enum { LJ_BS = 0x5C };   /* backslash */
static int lj_hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static int lj_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int lj_str_rtf_strip(char *dst, size_t cap, const char *rtf)
{
    static const char *const skip[] = { "fonttbl", "colortbl", "stylesheet", "info", "pict", "header", "footer", NULL };
    const unsigned char *s = (const unsigned char *)rtf;
    size_t o = 0;
    int depth = 0, skipat = 0, first = 0, trunc = 0;
    if (!dst || !cap) return -1;
    while (s && *s) {
        int c = *s++, emit = -1, was_first = first;
        first = 0;
        if (c == '{') { depth++; first = 1; if (!skipat && s[0] == LJ_BS && s[1] == '*') skipat = depth; continue; }
        if (c == '}') { if (depth == skipat) skipat = 0; if (depth) depth--; continue; }
        if (c == '\r' || c == '\n') continue;
        if (c != LJ_BS) emit = c;
        else if (*s == '\'' && lj_hex(s[1]) >= 0 && lj_hex(s[2]) >= 0) { emit = lj_hex(s[1]) * 16 + lj_hex(s[2]); s += 3; }
        else if (*s == LJ_BS || *s == '{' || *s == '}') emit = *s++;
        else if (lj_alpha(*s)) {
            char w[16];
            int n = 0, i;
            for (; lj_alpha(*s); s++) if (n < 15) w[n++] = (char)*s;
            w[n] = 0;
            if (*s == '-') s++;
            while (*s >= '0' && *s <= '9') s++;
            if (*s == ' ') s++;
            if (!strcmp(w, "par") || !strcmp(w, "line")) emit = '\n';
            else if (!strcmp(w, "tab")) emit = '\t';
            for (i = 0; was_first && !skipat && skip[i]; i++) if (!strcmp(w, skip[i])) skipat = depth;
        } else if (*s) s++;                                          /* other control symbol (\~ \- \_ ...) */
        if (emit < 0 || skipat) continue;
        if (o + 1 < cap) dst[o++] = (char)emit; else trunc = 1;
    }
    dst[o] = 0;
    return trunc ? -1 : 0;
}
