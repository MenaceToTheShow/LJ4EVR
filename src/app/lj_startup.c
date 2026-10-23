/* lj_startup.c - see lj_startup.h. */
#include "app/lj_startup.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_restore_data.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "version.h"
#include "windows/lj_file_dialogs.h"

int lj_startup_library(const char *userdir, const char *u_arg, lj_library *out)
{
    char name[LJ_LIBNAME_CAP], path[LJ_PATH_CAP];
    size_t n;
    int rc;

    lj_str_copy(name, sizeof name, "Default");
    if (lj_restore_data_requested(userdir)) {                                  /* System > Restore Program Data asked for it (W4-DLG-2) */
        if (lj_restore_data_path(userdir, path, sizeof path) != 0 || !lj_restore_data_exists(userdir)) lj_restore_data_request(userdir, 0);
        else if (lj_restore_data_apply(userdir, path, out) == 0) {
            lj_restore_data_request(userdir, 0);
            lj_library_select(userdir, out->name);
            lj_log("Library Restored: %s", out->name);                        /* exe 0x150D34 */
            return 0;
        }
        else lj_log("startup: restore data %s did not pass the check; RESTORE=1 kept for the error prompt", path);   /* lj_file_dlg_startup asks abort / keep */
    }
    if (u_arg && u_arg[0]) {                                                   /* `lj4evr U:<folder>` [01 s6.2]: the lj_library_create step, so the MIDIIn folder and the rest exist (ME-130) */
        lj_str_copy(name, sizeof name, u_arg);
        n = strlen(name);
        while (n > 0 && (name[n - 1] == '\\' || name[n - 1] == '/')) name[--n] = 0;   /* a trailing separator as Startup.ini spells Start= */
        if (lj_library_create(userdir, name, out) == 0) return 0;
        lj_log("startup: U:%s is not a usable library name, opening Default", u_arg);
        lj_str_copy(name, sizeof name, "Default");
    }
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


lj_window *lj_startup_status_create(void)
{
    static const lj_binding b[] = { { "FormClose", lj_handler_noop, NULL }, { "FormCreate", lj_handler_noop, NULL },
                                    { "Re1MouseDown", lj_handler_noop, NULL }, { "Timer1Timer", lj_handler_noop, NULL }, { NULL, NULL, NULL } };
    return lj_window_create(&lj_form_TSTARTUPSTATUSWINDOW, b);
}

/* TStartupStatusWindow "Startup Status" (DFM caption, D2): Re1 read-only, one log line per row while the library loads.
 * The lines are kept by the FILES family (lj_file_dlg_status_add) so LJ_WIN_PLEASE_WAIT shows them again with the
 * self-destruct countdown after the load. */
void lj_startup_status_line(lj_window *status, const char *line)
{
    const char *const *rows;
    int n;
    lj_widget *re = status ? lj_window_find(status, "Re1") : NULL;
    if (!re || !line) return;
    lj_file_dlg_status_add(line);
    n = lj_file_dlg_status_rows(&rows);
    lj_w_memo_set_lines(re, (const char **)rows, n);
}
