/* lj_restore_data.c - see lj_restore_data.h. */
#define _CRT_SECURE_NO_WARNINGS 1   /* localtime: UI thread only */
#include "model/lj_restore_data.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "core/lj_fsinfo.h"
#include "core/lj_ini.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_kv.h"

int lj_restore_data_path(const char *userdir, char *out, size_t cap)
{
    size_t n = strlen(userdir);
    int sep = n && (userdir[n - 1] == '/' || userdir[n - 1] == '\\');
    return snprintf(out, cap, "%s%s" LJ_RESTORE_DATA_FILE, userdir, n && !sep ? "/" : "") < (int)cap ? 0 : -1;
}

int lj_restore_data_exists(const char *userdir)
{
    char path[LJ_PATH_CAP];
    FILE *f;
    if (lj_restore_data_path(userdir, path, sizeof path) != 0 || (f = fopen(path, "rb")) == NULL) return 0;
    fclose(f);
    return 1;
}

int lj_restore_data_create(const lj_library *lib, const char *path)
{
    char tmp[LJ_PATH_CAP], name[LJ_LIBNAME_CAP + 8], when[32], manifest[LJ_LIBNAME_CAP + 96];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp || lj_kv_quote(name, sizeof name, lib->name) != 0) return -1;
    if (!tm || !strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", tm)) when[0] = 0;
    snprintf(manifest, sizeof manifest, "LJ4 restore %d\nlibrary=%s\ncreated=%s\n", LJ_RESTORE_FORMAT, name, when);
    if (lj_library_backup(lib, tmp, manifest) != 0) return -1;            /* removes tmp on failure */
    return lj_fs_replace(tmp, path);                           /* ME-139: a failed rename keeps tmp, the only new copy */
}

int lj_restore_data_info(const char *path, char *library, size_t cap)
{
    char notes[512], *line, *next;
    int fmt = -1;
    library[0] = 0;
    if (lj_library_backup_notes(path, notes, sizeof notes) != 0) return -1;
    for (line = notes; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        if (line[0] && line[strlen(line) - 1] == '\r') line[strlen(line) - 1] = 0;
        if (strncmp(line, "LJ4 restore ", 12) == 0) { if (sscanf(line + 12, "%d", &fmt) != 1) fmt = -1; }
        else if (strncmp(line, "library=", 8) == 0 && lj_kv_unquote(library, cap, line + 8) != 0) library[0] = 0;
    }
    if (fmt < 1 || fmt > LJ_RESTORE_FORMAT) { lj_log("restore data: %s has no \"LJ4 restore %d\" manifest", path, LJ_RESTORE_FORMAT); library[0] = 0; return -1; }
    if (!library[0] || strpbrk(library, "\\/:*?\"<>|") || strcmp(library, ".") == 0 || strcmp(library, "..") == 0) { lj_log("restore data: %s names no valid library", path); library[0] = 0; return -1; }
    return 0;
}

int lj_restore_data_apply(const char *userdir, const char *path, lj_library *out)
{
    char name[LJ_LIBNAME_CAP];
    if (lj_restore_data_info(path, name, sizeof name) != 0) return -1;
    if (lj_library_open(userdir, name, out) != 0 && lj_library_create(userdir, name, out) != 0) return -1;
    lj_log("Restoring data files - please wait");                       /* exe 0x2326A0 */
    return lj_library_replace(out, path);                                /* a failed check ("Restore data file seems to be corrupt") erases nothing */
}

static int lj_startup_set(const char *userdir, const char *key, const char *val)
{
    char path[LJ_PATH_CAP];
    lj_ini *ini;
    int r;
    if (lj_library_startup_ini(userdir, path, sizeof path) != 0) return -1;
    if ((ini = lj_ini_load(path)) == NULL) ini = lj_ini_new();
    lj_ini_sets(ini, "RESTORELIB", key, val);
    r = lj_ini_save(ini, path);
    lj_ini_free(ini);
    return r;
}

static void lj_startup_get(const char *userdir, const char *key, char *out, size_t cap)
{
    char path[LJ_PATH_CAP];
    lj_ini *ini;
    out[0] = 0;
    if (lj_library_startup_ini(userdir, path, sizeof path) != 0 || (ini = lj_ini_load(path)) == NULL) return;
    lj_str_copy(out, cap, lj_ini_gets(ini, "RESTORELIB", key, ""));
    lj_ini_free(ini);
}

int lj_restore_data_request(const char *userdir, int on) { return lj_startup_set(userdir, "RESTORE", on ? "1" : "0"); }
int lj_restore_data_requested(const char *userdir) { char v[8]; lj_startup_get(userdir, "RESTORE", v, sizeof v); return strcmp(v, "1") == 0; }

static void lj_pass_hash(const char *pw, char *out, size_t cap)   /* FNV-1a 32 over the CP1252 bytes: hides the text, protects nothing */
{
    unsigned long h = 2166136261UL;
    for (; *pw; pw++) h = ((h ^ (unsigned char)*pw) * 16777619UL) & 0xFFFFFFFFUL;
    snprintf(out, cap, "%08lX", h);
}

int lj_restore_password_set(const char *userdir, const char *pw)
{
    char h[16] = "";
    if (pw && strlen(pw) > LJ_RESTORE_PASS_MAX) return -1;
    if (pw && pw[0]) lj_pass_hash(pw, h, sizeof h);
    return lj_startup_set(userdir, "Pass", h);
}

int lj_restore_password_isset(const char *userdir) { char v[16]; lj_startup_get(userdir, "Pass", v, sizeof v); return v[0] != 0; }

int lj_restore_password_check(const char *userdir, const char *pw)
{
    char want[16], got[16];
    lj_startup_get(userdir, "Pass", want, sizeof want);
    if (!want[0]) return 1;
    lj_pass_hash(pw ? pw : "", got, sizeof got);
    return strcmp(want, got) == 0;
}
