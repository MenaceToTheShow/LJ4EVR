/* lj_fsinfo.c - lj_fsinfo.h: stat() and mkdir() behind two calls (the only sys/ headers outside lj_library.c). */
#define _CRT_SECURE_NO_WARNINGS 1   /* localtime / stat / mkdir are fine on the UI thread */
#include "core/lj_fsinfo.h"
#include "core/lj_log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

int lj_fs_mtime_text(const char *path, char *out, size_t cap)
{
    struct stat st;
    struct tm *t;
    if (cap) out[0] = 0;
    if (!path || stat(path, &st) != 0 || (t = localtime(&st.st_mtime)) == NULL) return -1;   /* ponytail: localtime is not reentrant; UI thread only */
    return snprintf(out, cap, "%02d-%02d-%04d %02d:%02d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900, t->tm_hour, t->tm_min) < (int)cap ? 0 : -1;
}

int lj_fs_mkdir(const char *path)
{
    struct stat st;
#ifdef _WIN32
    if (path && stat(path, &st) == 0 && (st.st_mode & _S_IFMT) == _S_IFDIR) return 0;
    return path && _mkdir(path) == 0 ? 0 : -1;
#else
    if (path && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;   /* S_IFMT is hidden under strict -std=c11 on glibc */
    return path && mkdir(path, 0777) == 0 ? 0 : -1;
#endif
}

unsigned lj_fs_exists_calls;

int lj_fs_exists(const char *path) { FILE *f; lj_fs_exists_calls++; f = path ? fopen(path, "rb") : NULL; if (!f) return 0; fclose(f); return 1; }

static int lj_fs_sep(char c) { return c == '/' || c == 0x5C; }
static int lj_fs_up(char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

int lj_fs_media_path(const char *root, const char *path, char *out, size_t cap)
{
    static const char *const dev[] = { "CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$" };
    size_t n = path ? strlen(path) : 0, i, a, b, k;
    if (cap) out[0] = 0;
    if (n < 5 || (lj_fs_sep(path[0]) && lj_fs_sep(path[1]))) return -1;              /* UNC (an SMB connect) and the ? / . device namespaces */
    if (path[n - 4] != '.' || lj_fs_up(path[n - 3]) != 'W' || lj_fs_up(path[n - 2]) != 'A' || lj_fs_up(path[n - 1]) != 'V') return -1;
    for (i = 0; i < n; i++) if (path[i] == ':' && (i != 1 || lj_fs_up(path[0]) < 'A' || lj_fs_up(path[0]) > 'Z')) return -1;   /* streams, devices */
    for (a = 0; a < n; a = b + 1) {                                                   /* every component's stem: up to the first '.', trailing spaces cut */
        for (b = a; b < n && !lj_fs_sep(path[b]); b++) {}
        for (i = a; i < b && path[i] != '.'; i++) {}
        while (i > a && path[i - 1] == ' ') i--;
        for (k = 0; k < sizeof dev / sizeof dev[0]; k++) {
            size_t m = strlen(dev[k]), j;
            for (j = 0; j < m && a + j < i && lj_fs_up(path[a + j]) == dev[k][j]; j++) {}
            if (j == m && i - a == m) return -1;
        }
        if (i - a == 4 && ((lj_fs_up(path[a]) == 'C' && lj_fs_up(path[a + 1]) == 'O' && lj_fs_up(path[a + 2]) == 'M') ||
                           (lj_fs_up(path[a]) == 'L' && lj_fs_up(path[a + 1]) == 'P' && lj_fs_up(path[a + 2]) == 'T')) &&
            ((path[a + 3] >= '0' && path[a + 3] <= '9') || (unsigned char)path[a + 3] == 0xB9 || (unsigned char)path[a + 3] == 0xB2 || (unsigned char)path[a + 3] == 0xB3)) return -1;
    }
    if (lj_fs_sep(path[0]) || path[1] == ':' || !root) root = "";                    /* absolute: as given */
    return (size_t)snprintf(out, cap, "%s%s", root, path) < cap ? 0 : (out[0] = 0, -1);
}

int lj_fs_replace(const char *tmp, const char *path)
{
    remove(path);                                              /* Windows rename does not replace */
    if (rename(tmp, path) == 0) return 0;
    lj_log("file: cannot replace %s; the new copy is kept in %s", path, tmp);   /* a scanner holding the fresh file, a folder in the way */
    return -1;
}
