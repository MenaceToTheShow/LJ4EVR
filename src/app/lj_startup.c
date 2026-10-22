/* lj_startup.c - see lj_startup.h. */
#include "app/lj_startup.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "version.h"

int lj_startup_library(const char *userdir, const char *u_arg, lj_library *out)
{
    char name[LJ_LIBNAME_CAP], path[LJ_PATH_CAP];
    size_t n;
    int rc;

    lj_str_copy(name, sizeof name, "Default");
    if (u_arg && u_arg[0]) lj_str_copy(name, sizeof name, u_arg);               /* `lj4evr U:<folder>` [01 s6.2] */
    else if (snprintf(path, sizeof path, "%sStartup.ini", userdir) < (int)sizeof path) {
        lj_ini *ini = lj_ini_load(path);
        if (ini) { lj_str_copy(name, sizeof name, lj_ini_gets(ini, "USERDIRECTORY", "Start", "Default\\")); lj_ini_free(ini); }
    }
    n = strlen(name);
    while (n > 0 && (name[n - 1] == '\\' || name[n - 1] == '/')) name[--n] = 0;   /* shipped value is "Default\\" */
    if (!name[0]) lj_str_copy(name, sizeof name, "Default");

    rc = strcmp(name, "Default") == 0 ? lj_library_open_default(userdir, out) : lj_library_open(userdir, name, out);
    if (rc != 0 && strcmp(name, "Default") != 0) {
        lj_log("startup: user library %s not found, opening Default", name);
        rc = lj_library_open_default(userdir, out);
    }
    if (rc != 0) {
        lj_log("startup: cannot open the Default library under %s; using the folder itself", userdir);
        memset(out, 0, sizeof *out);
        lj_str_copy(out->userdir, sizeof out->userdir, userdir);
        lj_str_copy(out->root, sizeof out->root, userdir);
        lj_str_copy(out->name, sizeof out->name, "Default");
        return -1;
    }
    return 0;
}

void lj_startup_last_run(lj_ini *main_ini)
{
    if (!main_ini) return;
    lj_ini_sets(main_ini, "OTHER", "LastRun", "LJ4EVR " LJ_VERSION_STR);
    lj_ini_seti(main_ini, "OTHER", "FirstRun", 0);
}

static void lj_startup_noop(lj_widget *w, void *ud) { (void)w; (void)ud; }

lj_window *lj_startup_status_create(void)
{
    static const lj_binding b[] = { { "FormClose", lj_startup_noop, NULL }, { "FormCreate", lj_startup_noop, NULL },
                                    { "Re1MouseDown", lj_startup_noop, NULL }, { "Timer1Timer", lj_startup_noop, NULL }, { NULL, NULL, NULL } };
    return lj_window_create(&lj_form_TSTARTUPSTATUSWINDOW, b);
}

/* TStartupStatusWindow "Startup Status" (DFM caption, D2): Re1 read-only, one log line per row while the library loads. */
void lj_startup_status_line(lj_window *status, const char *line)
{
    static char text[LJ_STARTUP_LINES][160];
    static const char *rows[LJ_STARTUP_LINES];
    static int n;
    int i;
    lj_widget *re = status ? lj_window_find(status, "Re1") : NULL;
    if (!re || !line) return;
    if (n == LJ_STARTUP_LINES) { memmove(text[0], text[1], sizeof text - sizeof text[0]); n--; }
    lj_str_copy(text[n++], sizeof text[0], line);
    for (i = 0; i < n; i++) rows[i] = text[i];
    lj_w_memo_set_lines(re, rows, n);
}
