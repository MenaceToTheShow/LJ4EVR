/* lj_library.c - the user library folder (lj_library.h, design 5.1/5.2, 01 s8, 18 s2).
 * Folder names are LJ2's; numbering is DirInfo.Ini [NEXT] (ours). */
#define _CRT_SECURE_NO_WARNINGS 1   /* fopen/localtime under /W4 /WX on MSVC */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define LJ_SEP '\\'
#else
#include <dirent.h>
#include <sys/stat.h>   /* mkdir; not yet on tools/test_layering.py's OS_ALLOW for this file (reported) */
#define LJ_SEP '/'
#endif
#include "model/lj_library.h"
#include "model/lj_model.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "miniz.h"

static const char *const lj_lib_folders[] = { "Sequences", "Cues", "CueLists", "CueMacros", "BGCues", "UserFixtureDef", "MIDIIn", "Presets", "Palettes", NULL };
static const char *const lj_rec_folder[LJ_REC_KIND_COUNT] = { "Sequences", "Cues", "CueLists", "BGCues", "CueMacros" };
static const char *const lj_rec_ext[LJ_REC_KIND_COUNT]    = { ".seq", ".cue", ".Cl2", ".bgc", ".CMA" };
static const char *const lj_rec_key[LJ_REC_KIND_COUNT]    = { "Seq", "Cue", "Cl", "Bg", "Macro" };

/* ---- the few filesystem calls, both CRTs ---- */

static int lj_fs_mkdir(const char *path)
{
#ifdef _WIN32
    int r = _mkdir(path);
#else
    int r = mkdir(path, 0777);
#endif
    return (r == 0 || errno == EEXIST) ? 0 : -1;
}

static int lj_fs_rmdir(const char *path)
{
#ifdef _WIN32
    return _rmdir(path) == 0 ? 0 : -1;
#else
    return remove(path) == 0 ? 0 : -1;
#endif
}

typedef struct {
#ifdef _WIN32
    intptr_t h; struct _finddata_t fd; int first;
#else
    DIR *d; char path[LJ_PATH_CAP];
#endif
} lj_dir;

static int lj_dir_open(lj_dir *d, const char *path)
{
#ifdef _WIN32
    char pat[LJ_PATH_CAP];
    if (snprintf(pat, sizeof pat, "%s%c*", path, LJ_SEP) >= (int)sizeof pat) return -1;
    d->h = _findfirst(pat, &d->fd);
    d->first = 1;
    return d->h == -1 ? -1 : 0;
#else
    d->d = opendir(path);
    lj_str_copy(d->path, sizeof d->path, path);
    return d->d ? 0 : -1;
#endif
}

static int lj_dir_next(lj_dir *d, char *name, size_t cap, int *is_dir)   /* 1 entry, 0 end; "." and ".." skipped */
{
    for (;;) {
#ifdef _WIN32
        if (d->first) d->first = 0;
        else if (_findnext(d->h, &d->fd) != 0) return 0;
        if (strcmp(d->fd.name, ".") == 0 || strcmp(d->fd.name, "..") == 0) continue;
        lj_str_copy(name, cap, d->fd.name);
        *is_dir = (d->fd.attrib & _A_SUBDIR) != 0;
        return 1;
#else
        struct dirent *e = readdir(d->d);
        if (!e) return 0;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        lj_str_copy(name, cap, e->d_name);
        {
            char full[LJ_PATH_CAP];
            DIR *sub;
            snprintf(full, sizeof full, "%s%c%s", d->path, LJ_SEP, e->d_name);
            sub = opendir(full);                  /* d_type is not portable across file systems */
            *is_dir = sub != NULL;
            if (sub) closedir(sub);
        }
        return 1;
#endif
    }
}

static void lj_dir_close(lj_dir *d)
{
#ifdef _WIN32
    _findclose(d->h);
#else
    closedir(d->d);
#endif
}

static int lj_fs_is_dir(const char *path) { lj_dir d; if (lj_dir_open(&d, path) != 0) return 0; lj_dir_close(&d); return 1; }

/* Post-order walk: fn(ud, abs, rel, is_dir) for every file, then each directory (rel uses '/'). Stops at the first -1. */
static int lj_walk(const char *abs, const char *rel, int (*fn)(void *ud, const char *abs, const char *rel, int is_dir), void *ud)
{
    lj_dir d;
    char name[260], sub_abs[LJ_PATH_CAP], sub_rel[LJ_PATH_CAP];
    int is_dir, r = 0;
    if (lj_dir_open(&d, abs) != 0) return -1;
    while (r == 0 && lj_dir_next(&d, name, sizeof name, &is_dir) == 1) {
        if (snprintf(sub_abs, sizeof sub_abs, "%s%c%s", abs, LJ_SEP, name) >= (int)sizeof sub_abs
            || snprintf(sub_rel, sizeof sub_rel, "%s%s%s", rel, *rel ? "/" : "", name) >= (int)sizeof sub_rel) { r = -1; break; }
        if (is_dir) r = lj_walk(sub_abs, sub_rel, fn, ud);
        if (r == 0) r = fn(ud, sub_abs, sub_rel, is_dir);
    }
    lj_dir_close(&d);
    return r;
}

static int lj_rm_cb(void *ud, const char *abs, const char *rel, int is_dir)
{
    (void)ud; (void)rel;
    return is_dir ? lj_fs_rmdir(abs) : (remove(abs) == 0 ? 0 : -1);
}

static int lj_fs_rmtree(const char *path) { return lj_walk(path, "", lj_rm_cb, NULL) == 0 && lj_fs_rmdir(path) == 0 ? 0 : -1; }

/* ---- names and paths ---- */

static int lj_name_ok(const char *name)
{
    size_t n = strlen(name);
    if (!n || n >= LJ_LIBNAME_CAP || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    return !strpbrk(name, "\\/:*?\"<>|");
}

static int lj_ieq(const char *a, const char *b)                      /* the folder name is case-insensitive on Windows */
{
    for (; *a && *b; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return *a == *b;
}

static int lj_userdir_user(const char *userdir, char *out, size_t cap)     /* <userdir>User (no trailing separator) */
{
    size_t n = strlen(userdir);
    int sep = n && (userdir[n - 1] == '/' || userdir[n - 1] == '\\');
    return snprintf(out, cap, "%s%s%s", userdir, sep ? "" : "\\", "User") < (int)cap ? 0 : -1;
}

static void lj_fix_seps(char *s) { for (; *s; s++) if (*s == '/' || *s == '\\') *s = LJ_SEP; }

static int lj_library_fill(const char *userdir, const char *name, lj_library *out)
{
    char user[LJ_PATH_CAP];
    if (!lj_name_ok(name) || lj_userdir_user(userdir, user, sizeof user) != 0) return -1;
    if (lj_str_copy(out->userdir, sizeof out->userdir, userdir) != 0) return -1;
    lj_str_copy(out->name, sizeof out->name, name);
    if (snprintf(out->root, sizeof out->root, "%s%c%s%c", user, LJ_SEP, name, LJ_SEP) >= (int)sizeof out->root) return -1;
    lj_fix_seps(out->root);
    return 0;
}

int lj_library_open(const char *userdir, const char *name, lj_library *out)
{
    char root[LJ_PATH_CAP];
    if (lj_library_fill(userdir, name, out) != 0) { lj_log("library: bad name or path \"%s\"", name); return -1; }
    lj_str_copy(root, sizeof root, out->root);
    root[strlen(root) - 1] = 0;
    if (!lj_fs_is_dir(root)) { lj_log("library: no folder %s", root); return -1; }
    return 0;
}

int lj_library_create(const char *userdir, const char *name, lj_library *out)
{
    char path[LJ_PATH_CAP], stamp[32];
    int i;
    lj_ini *ini;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (lj_library_fill(userdir, name, out) != 0) { lj_log("library: bad name or path \"%s\"", name); return -1; }
    lj_str_copy(path, sizeof path, out->userdir);
    if (lj_fs_mkdir(path) != 0) { lj_log("library: cannot create %s", path); return -1; }
    lj_userdir_user(out->userdir, path, sizeof path); lj_fix_seps(path);
    if (lj_fs_mkdir(path) != 0) { lj_log("library: cannot create %s", path); return -1; }
    lj_str_copy(path, sizeof path, out->root);
    path[strlen(path) - 1] = 0;
    if (lj_fs_mkdir(path) != 0) { lj_log("library: cannot create %s", path); return -1; }
    for (i = 0; lj_lib_folders[i]; i++) {
        lj_library_path(out, lj_lib_folders[i], path, sizeof path);
        if (lj_fs_mkdir(path) != 0) { lj_log("library: cannot create %s", path); return -1; }
    }
    lj_library_path(out, "DirInfo.Ini", path, sizeof path);
    ini = lj_ini_load(path);
    if (!ini) {
        ini = lj_ini_new();
        lj_ini_sets(ini, "INFO", "Name", name);
        if (tm && strftime(stamp, sizeof stamp, "%m/%d/%Y", tm)) lj_ini_sets(ini, "INFO", "Date", stamp);   /* 18 s2: Date=M/D/YYYY */
        for (i = 0; i < LJ_REC_KIND_COUNT; i++) lj_ini_seti(ini, "NEXT", lj_rec_key[i], 1);
        if (lj_ini_save(ini, path) != 0) { lj_ini_free(ini); return -1; }
    }
    lj_ini_free(ini);
    return 0;
}

int lj_library_open_default(const char *userdir, lj_library *out)
{
    if (lj_library_open(userdir, "Default", out) == 0) return 0;
    lj_log("library: recreating the Default library [01 s8]");
    return lj_library_create(userdir, "Default", out);
}

int lj_library_list(const char *userdir, char names[][LJ_LIBNAME_CAP], int max)
{
    char user[LJ_PATH_CAP], name[260];
    lj_dir d;
    int n = 0, is_dir;
    if (lj_userdir_user(userdir, user, sizeof user) != 0) return -1;
    lj_fix_seps(user);
    if (lj_dir_open(&d, user) != 0) { lj_log("library: cannot list %s", user); return -1; }
    while (lj_dir_next(&d, name, sizeof name, &is_dir) == 1)
        if (is_dir && lj_name_ok(name) && n < max) lj_str_copy(names[n++], LJ_LIBNAME_CAP, name);
    lj_dir_close(&d);
    return n;
}

int lj_library_delete(const char *userdir, const char *name)
{
    lj_library lib;
    char root[LJ_PATH_CAP];
    if (lj_ieq(name, "Default")) { lj_log("library: Default cannot be deleted [01 s8]"); return -1; }
    if (lj_library_open(userdir, name, &lib) != 0) return -1;
    lj_str_copy(root, sizeof root, lib.root);
    root[strlen(root) - 1] = 0;
    if (lj_fs_rmtree(root) != 0) { lj_log("library: cannot delete %s", root); return -1; }
    return 0;
}

int lj_library_rename(const char *userdir, const char *from, const char *to)
{
    lj_library a, b;
    char ra[LJ_PATH_CAP], rb[LJ_PATH_CAP];
    if (lj_ieq(from, "Default") || lj_ieq(to, "Default")) { lj_log("library: Default cannot be renamed [01 s8]"); return -1; }
    if (lj_library_open(userdir, from, &a) != 0 || lj_library_fill(userdir, to, &b) != 0) return -1;
    lj_str_copy(ra, sizeof ra, a.root); ra[strlen(ra) - 1] = 0;
    lj_str_copy(rb, sizeof rb, b.root); rb[strlen(rb) - 1] = 0;
    if (lj_fs_is_dir(rb) || rename(ra, rb) != 0) { lj_log("library: cannot rename %s to %s", ra, rb); return -1; }
    return 0;
}

int lj_library_is_default(const lj_library *lib) { return lj_ieq(lib->name, "Default"); }

int lj_library_path(const lj_library *lib, const char *rel, char *out, size_t cap)
{
    int n = snprintf(out, cap, "%s%s", lib->root, rel);
    lj_fix_seps(out);
    return n < (int)cap ? 0 : -1;
}

int lj_library_record_path(const lj_library *lib, enum lj_rec_kind kind, uint32_t number, char *out, size_t cap)
{
    char rel[64];
    if ((int)kind < 0 || kind >= LJ_REC_KIND_COUNT || !number || number > (uint32_t)LJ_NUMBER_MAX) { if (cap) out[0] = 0; return -1; }
    snprintf(rel, sizeof rel, "%s%c%08u%s", lj_rec_folder[kind], LJ_SEP, (unsigned)number, lj_rec_ext[kind]);   /* 8-digit names [F28] */
    return lj_library_path(lib, rel, out, cap);
}

static int lj_cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

int lj_library_list_records(const lj_library *lib, enum lj_rec_kind kind, uint32_t *numbers, int max)
{
    char dir[LJ_PATH_CAP], name[260];
    lj_dir d;
    int n = 0, is_dir, i;
    if ((int)kind < 0 || kind >= LJ_REC_KIND_COUNT || lj_library_path(lib, lj_rec_folder[kind], dir, sizeof dir) != 0) return -1;
    if (lj_dir_open(&d, dir) != 0) return -1;
    while (lj_dir_next(&d, name, sizeof name, &is_dir) == 1) {
        if (is_dir || strlen(name) != 8 + strlen(lj_rec_ext[kind]) || !lj_ieq(name + 8, lj_rec_ext[kind])) continue;
        for (i = 0; i < 8 && name[i] >= '0' && name[i] <= '9'; i++) ;
        if (i < 8 || n >= max) continue;
        numbers[n++] = (uint32_t)strtoul(name, NULL, 10);
    }
    lj_dir_close(&d);
    qsort(numbers, (size_t)n, sizeof *numbers, lj_cmp_u32);
    return n;
}

uint32_t lj_library_next_number(lj_library *lib, enum lj_rec_kind kind)
{
    char path[LJ_PATH_CAP];
    lj_ini *ini;
    int next;
    uint32_t r = 0;
    if ((int)kind < 0 || kind >= LJ_REC_KIND_COUNT || lj_library_path(lib, "DirInfo.Ini", path, sizeof path) != 0) return 0;
    ini = lj_ini_load(path);
    if (!ini) ini = lj_ini_new();
    next = lj_ini_geti(ini, "NEXT", lj_rec_key[kind], 0);
    if (next < 1) {                                                /* key missing: max existing + 1 (PROPOSAL, design 5.2) */
        uint32_t *nums = (uint32_t *)malloc((size_t)LJ_MACROS_MAX * sizeof *nums);
        int n;
        if (!nums) lj_fatal("lj_library_next_number: out of memory");
        n = lj_library_list_records(lib, kind, nums, LJ_MACROS_MAX);
        next = n > 0 ? (int)nums[n - 1] + 1 : 1;
        free(nums);
        lj_log("library: DirInfo.Ini [NEXT] %s missing, recovered as %d", lj_rec_key[kind], next);
    }
    if (next <= (int)LJ_NUMBER_MAX) {
        lj_ini_seti(ini, "NEXT", lj_rec_key[kind], next + 1);
        if (lj_ini_save(ini, path) == 0) r = (uint32_t)next;
        else lj_log("library: cannot write %s", path);
    }
    lj_ini_free(ini);
    return r;
}

int lj_library_delete_record(const lj_library *lib, enum lj_rec_kind kind, uint32_t number)
{
    char path[LJ_PATH_CAP];
    if (lj_library_record_path(lib, kind, number, path, sizeof path) != 0) return -1;
    if (remove(path) != 0) { lj_log("library: cannot delete %s", path); return -1; }
    return 0;
}

/* ---- backup / restore [F51]: a zip through miniz, entries relative to the library root ---- */

static int lj_zip_cb(void *ud, const char *abs, const char *rel, int is_dir)
{
    if (is_dir) return 0;
    return mz_zip_writer_add_file((mz_zip_archive *)ud, rel, abs, NULL, 0, MZ_DEFAULT_LEVEL) ? 0 : -1;
}

int lj_library_backup(const lj_library *lib, const char *zip_path, const char *notes)
{
    mz_zip_archive zip;
    char root[LJ_PATH_CAP];
    int ok;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_writer_init_file(&zip, zip_path, 0)) { lj_log("library: cannot write %s", zip_path); return -1; }
    lj_str_copy(root, sizeof root, lib->root);
    root[strlen(root) - 1] = 0;
    ok = lj_walk(root, "", lj_zip_cb, &zip) == 0;
    if (!notes) notes = "";
    ok = ok && mz_zip_writer_add_mem(&zip, "notes.txt", notes, strlen(notes), MZ_DEFAULT_LEVEL);
    ok = ok && mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    if (!ok) { lj_log("library: backup to %s failed", zip_path); remove(zip_path); return -1; }
    return 0;
}

int lj_library_restore(const lj_library *lib, const char *zip_path)
{
    mz_zip_archive zip;
    mz_uint i, n;
    int ok = 1;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) { lj_log("library: cannot read %s", zip_path); return -1; }
    n = mz_zip_reader_get_num_files(&zip);
    for (i = 0; ok && i < n; i++) {
        mz_zip_archive_file_stat st;
        char dst[LJ_PATH_CAP], *p;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) { ok = 0; break; }
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;
        if (st.m_filename[0] == '/' || st.m_filename[0] == '\\' || strchr(st.m_filename, ':') || strstr(st.m_filename, "..")) {   /* the archive is untrusted input */
            lj_log("library: restore refuses entry \"%s\"", st.m_filename);
            ok = 0; break;
        }
        if (lj_library_path(lib, st.m_filename, dst, sizeof dst) != 0) { ok = 0; break; }
        for (p = dst + strlen(lib->root); (p = strchr(p, LJ_SEP)) != NULL; p++) {   /* create the entry's folders */
            *p = 0;
            if (lj_fs_mkdir(dst) != 0) ok = 0;
            *p = LJ_SEP;
        }
        ok = ok && mz_zip_reader_extract_to_file(&zip, i, dst, 0);
    }
    mz_zip_reader_end(&zip);
    if (!ok) lj_log("library: restore from %s failed", zip_path);
    return ok ? 0 : -1;
}

int lj_library_tick_hz(lj_ini *main_ini)
{
    int hz = lj_ini_geti(main_ini, "ENGINE", "TickHz", 40);     /* 40 = LJ_TICK_HZ_DEFAULT (D9) */
    int clamped = hz < 10 ? 10 : hz > 200 ? 200 : hz;             /* ME-24: 10..200 keeps the keep-alive inside 800-1000 ms */
    if (clamped != hz) lj_log("Main.Ini [ENGINE] TickHz=%d clamped to %d (ME-24)", hz, clamped);
    return clamped;
}

int lj_number_text(uint32_t number, char *out, size_t cap)
{
    return snprintf(out, cap, "%04u", (unsigned)number) < (int)cap ? 0 : -1;
}
