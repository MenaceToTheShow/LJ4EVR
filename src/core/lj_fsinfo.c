/* lj_fsinfo.c - lj_fsinfo.h: stat() and mkdir() behind two calls (the only sys/ headers outside lj_library.c). */
#define _CRT_SECURE_NO_WARNINGS 1   /* localtime / stat / mkdir are fine on the UI thread */
#include "core/lj_fsinfo.h"

#include <stdio.h>
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

int lj_fs_exists(const char *path) { FILE *f = path ? fopen(path, "rb") : NULL; if (!f) return 0; fclose(f); return 1; }
