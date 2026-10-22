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
#include "model/lj_cue.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "miniz.h"

static const char *const lj_lib_folders[] = { "Sequences", "Cues", "CueLists", "CueMacros", "BGCues", "UserFixtureDef", "MIDIIn", "Presets", "Palettes", "Hotkeys" /* lj_hotkeys_setup_path: <root>Hotkeys/SetupN.Hot (ours; docs/hotkeys.md) */, NULL };
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
            if (snprintf(full, sizeof full, "%s%c%s", d->path, LJ_SEP, e->d_name) >= (int)sizeof full) continue;   /* gcc -Wformat-truncation */
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

static int lj_startup_path(const char *userdir, char *out, size_t cap)
{
    size_t n = strlen(userdir);
    int sep = n && (userdir[n - 1] == '/' || userdir[n - 1] == '\\');
    if (snprintf(out, cap, "%s%sStartup.ini", userdir, sep ? "" : "\\") >= (int)cap) return -1;
    lj_fix_seps(out);
    return 0;
}

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
    {                                                            /* the deleted library was the start library: the next start opens Default [01 s8] */
        char sp[LJ_PATH_CAP], cur[LJ_LIBNAME_CAP + 2];
        lj_ini *ini;
        if (lj_startup_path(userdir, sp, sizeof sp) == 0 && (ini = lj_ini_load(sp)) != NULL) {
            lj_str_copy(cur, sizeof cur, lj_ini_gets(ini, "USERDIRECTORY", "Start", ""));
            if (cur[0] && strchr("/\\", cur[strlen(cur) - 1])) cur[strlen(cur) - 1] = 0;
            if (lj_ieq(cur, name)) { lj_ini_sets(ini, "USERDIRECTORY", "Start", "Default\\"); lj_ini_save(ini, sp); }
            lj_ini_free(ini);
        }
    }
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
        for (i = 0; i < 8 && name[i] >= '0' && name[i] <= '9'; i++) { }
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

#define LJ_BACKUP_NOTES "notes.txt"   /* the backup's own notes [F51]; never extracted (Notes.txt, the Notes window, is case-different and travels as a file) */
enum { LJ_RESTORE_MAX_BYTES = 512 * 1024 * 1024 };   /* PROPOSAL: a library is a few MB; caps a hostile archive's uncompressed total */

static int lj_zip_cb(void *ud, const char *abs, const char *rel, int is_dir)
{
    if (is_dir || strcmp(rel, LJ_BACKUP_NOTES) == 0) return 0;   /* an old W1 restore left the backup notes in the root */
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
    ok = ok && mz_zip_writer_add_mem(&zip, LJ_BACKUP_NOTES, notes, strlen(notes), MZ_DEFAULT_LEVEL);
    ok = ok && mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    if (!ok) { lj_log("library: backup to %s failed", zip_path); remove(zip_path); return -1; }
    return 0;
}

static int lj_zip_name_ok(const char *s)   /* the archive is untrusted input: relative, no "..", no drive or stream colon */
{
    return s[0] && s[0] != '/' && s[0] != '\\' && !strchr(s, ':') && !strstr(s, "..");
}

static int lj_mkdirs_for(char *path, size_t from)   /* create every folder of `path` after offset `from` */
{
    char *p;
    for (p = path + from; (p = strchr(p, LJ_SEP)) != NULL; p++) {
        *p = 0;
        if (lj_fs_mkdir(path) != 0) { *p = LJ_SEP; return -1; }
        *p = LJ_SEP;
    }
    return 0;
}

int lj_library_restore(const lj_library *lib, const char *zip_path)
{
    mz_zip_archive zip;
    mz_uint i, n;
    mz_uint64 total = 0;
    int ok = 1;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) { lj_log("library: cannot read %s", zip_path); return -1; }
    n = mz_zip_reader_get_num_files(&zip);
    for (i = 0; ok && i < n; i++) {                           /* pass 1: every entry is checked before the first byte is written */
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) { ok = 0; break; }
        total += st.m_uncomp_size;
        if (!lj_zip_name_ok(st.m_filename)) { lj_log("library: restore refuses entry \"%s\"", st.m_filename); ok = 0; }
        else if (total > (mz_uint64)LJ_RESTORE_MAX_BYTES) { lj_log("library: restore refuses %s (over %u bytes)", zip_path, (unsigned)LJ_RESTORE_MAX_BYTES); ok = 0; }
    }
    for (i = 0; ok && i < n; i++) {                           /* pass 2: extract */
        mz_zip_archive_file_stat st;
        char dst[LJ_PATH_CAP];
        if (!mz_zip_reader_file_stat(&zip, i, &st)) { ok = 0; break; }
        if (mz_zip_reader_is_file_a_directory(&zip, i) || strcmp(st.m_filename, LJ_BACKUP_NOTES) == 0) continue;
        if (lj_library_path(lib, st.m_filename, dst, sizeof dst) != 0 || lj_mkdirs_for(dst, strlen(lib->root)) != 0) { ok = 0; break; }
        ok = mz_zip_reader_extract_to_file(&zip, i, dst, 0);
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

/* ---- W2: the Backup / Restore / User Libraries / Delete Files dialogs ---- */

int lj_library_backup_notes(const char *zip_path, char *notes, size_t cap)
{
    mz_zip_archive zip;
    size_t len = 0;
    char *p;
    int ok;
    if (!notes || !cap) return -1;
    notes[0] = 0;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) { lj_log("library: %s is not a backup", zip_path); return -1; }
    p = (char *)mz_zip_reader_extract_file_to_heap(&zip, LJ_BACKUP_NOTES, &len, MZ_ZIP_FLAG_CASE_SENSITIVE);   /* never the library's Notes.txt */
    mz_zip_reader_end(&zip);
    if (!p) { lj_log("library: %s has no %s", zip_path, LJ_BACKUP_NOTES); return -1; }
    ok = len < cap;
    if (!ok) len = cap - 1;
    memcpy(notes, p, len);
    notes[len] = 0;
    mz_free(p);
    return 0;
}

int lj_library_backup_lists(const char *zip_path, int counts[LJ_REC_KIND_COUNT])
{
    mz_zip_archive zip;
    mz_uint i, n;
    int k;
    memset(counts, 0, sizeof(int) * LJ_REC_KIND_COUNT);
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) { lj_log("library: %s is not a backup", zip_path); return -1; }
    n = mz_zip_reader_get_num_files(&zip);
    for (i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        for (k = 0; k < LJ_REC_KIND_COUNT; k++) {             /* "<Folder>/NNNNNNNN.<ext>", the folder case-insensitive as on Windows */
            size_t fl = strlen(lj_rec_folder[k]), j;
            const char *s = st.m_filename;
            char head[32];
            if (strlen(s) != fl + 1 + 8 + strlen(lj_rec_ext[k]) || (s[fl] != '/' && s[fl] != '\\')) continue;
            memcpy(head, s, fl); head[fl] = 0;
            if (!lj_ieq(head, lj_rec_folder[k]) || !lj_ieq(s + fl + 9, lj_rec_ext[k])) continue;
            for (j = 0; j < 8 && s[fl + 1 + j] >= '0' && s[fl + 1 + j] <= '9'; j++) { }
            if (j == 8) counts[k]++;
        }
    }
    mz_zip_reader_end(&zip);
    return 0;
}

static int lj_copy_file(const char *src, const char *dst)
{
    char buf[65536];
    size_t n;
    int ok = 1;
    FILE *in = fopen(src, "rb"), *out = in ? fopen(dst, "wb") : NULL;
    if (!out) { if (in) fclose(in); return -1; }
    while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0) ok = fwrite(buf, 1, n, out) == n;
    ok = !ferror(in) && ok;
    fclose(in);
    return fclose(out) == 0 && ok ? 0 : -1;
}

static int lj_copy_cb(void *ud, const char *abs, const char *rel, int is_dir)
{
    const lj_library *to = (const lj_library *)ud;
    char dst[LJ_PATH_CAP];
    if (lj_library_path(to, rel, dst, sizeof dst) != 0) return -1;
    if (is_dir) return lj_fs_mkdir(dst);
    return lj_mkdirs_for(dst, strlen(to->root)) == 0 ? lj_copy_file(abs, dst) : -1;
}

static int lj_library_rename_info(const lj_library *lib)   /* DirInfo.Ini [INFO] Name = the folder's name; [NEXT] kept */
{
    char path[LJ_PATH_CAP];
    lj_ini *ini;
    int r;
    if (lj_library_path(lib, "DirInfo.Ini", path, sizeof path) != 0) return -1;
    ini = lj_ini_load(path);
    if (!ini) ini = lj_ini_new();
    lj_ini_sets(ini, "INFO", "Name", lib->name);
    r = lj_ini_save(ini, path);
    lj_ini_free(ini);
    return r;
}

int lj_library_copy(const char *userdir, const char *from, const char *to)
{
    lj_library a, b;
    char root[LJ_PATH_CAP];
    if (lj_library_open(userdir, from, &a) != 0) return -1;
    if (lj_library_open(userdir, to, &b) == 0) { lj_log("library: %s already exists", to); return -1; }
    if (lj_library_create(userdir, to, &b) != 0) return -1;
    lj_str_copy(root, sizeof root, a.root);
    root[strlen(root) - 1] = 0;
    if (lj_walk(root, "", lj_copy_cb, &b) != 0 || lj_library_rename_info(&b) != 0) {
        lj_log("library: copy of %s to %s failed", from, to);
        lj_library_delete(userdir, to);
        return -1;
    }
    return 0;
}

int lj_library_restore_new(const char *userdir, const char *name, const char *zip_path, lj_library *out)
{
    char try_name[LJ_LIBNAME_CAP];
    int k;
    lj_str_copy(try_name, sizeof try_name, name);
    for (k = 2; lj_library_open(userdir, try_name, out) == 0; k++) {   /* collision: "name (2)" .. "name (99)" (PROPOSAL rig) */
        if (k > 99 || snprintf(try_name, sizeof try_name, "%s (%d)", name, k) >= (int)sizeof try_name) { lj_log("library: no free name for %s", name); return -1; }
    }
    if (lj_library_create(userdir, try_name, out) != 0) return -1;
    if (lj_library_restore(out, zip_path) != 0 || lj_library_rename_info(out) != 0) {
        lj_library_delete(userdir, try_name);                   /* nothing left behind */
        return -1;
    }
    return 0;
}

int lj_library_select(const char *userdir, const char *name)
{
    char path[LJ_PATH_CAP], val[LJ_LIBNAME_CAP + 2];
    lj_library lib;
    lj_ini *ini;
    int r;
    if (lj_library_open(userdir, name, &lib) != 0 || lj_startup_path(userdir, path, sizeof path) != 0) return -1;
    snprintf(val, sizeof val, "%s\\", name);                   /* the shipped value is "Default\" */
    ini = lj_ini_load(path);
    if (!ini) ini = lj_ini_new();
    lj_ini_sets(ini, "USERDIRECTORY", "Start", val);
    r = lj_ini_save(ini, path);
    lj_ini_free(ini);
    return r;
}

static int lj_cmp_u32_key(const void *k, const void *e) { return lj_cmp_u32(k, e); }

int lj_library_used_by_cues(const lj_library *lib, enum lj_rec_kind kind, const uint32_t *numbers, int n, uint8_t *used)
{
    enum { LJ_SCAN_MAX = 65536 };   /* ponytail: cue files scanned per call; a bigger library needs a growable list */
    uint32_t *cues, refs[LJ_CUE_SLOTS];
    lj_cue *cue;
    char path[LJ_PATH_CAP], err[256];
    int nc, i, j, nr, count = 0;
    if ((kind != LJ_REC_SEQ && kind != LJ_REC_BG && kind != LJ_REC_MACRO) || n < 0 || (n && (!numbers || !used))) return -1;
    if (n) memset(used, 0, (size_t)n);
    cues = (uint32_t *)malloc(LJ_SCAN_MAX * sizeof *cues);
    if (!cues) lj_fatal("lj_library_used_by_cues: out of memory");
    nc = lj_library_list_records(lib, LJ_REC_CUE, cues, LJ_SCAN_MAX);
    if (nc < 0) { free(cues); return -1; }
    cue = lj_cue_new_rec();
    for (i = 0; i < nc; i++) {
        if (lj_library_record_path(lib, LJ_REC_CUE, cues[i], path, sizeof path) != 0 || lj_cue_load(path, cue, err, sizeof err) != 0) continue;   /* logged by the loader */
        nr = 0;
        if (kind == LJ_REC_SEQ) { for (j = 0; j < LJ_CUE_SLOTS; j++) if (cue->slot[j].seq) refs[nr++] = cue->slot[j].seq; }
        else refs[nr++] = kind == LJ_REC_BG ? cue->bg_cue : cue->gen_macro;
        for (j = 0; j < nr; j++) {
            const uint32_t *hit = refs[j] ? (const uint32_t *)bsearch(&refs[j], numbers, (size_t)n, sizeof *numbers, lj_cmp_u32_key) : NULL;
            if (hit && !used[hit - numbers]) { used[hit - numbers] = 1; count++; }
        }
    }
    lj_cue_free(cue);
    free(cues);
    return count;
}
