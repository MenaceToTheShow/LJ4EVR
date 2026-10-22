/* lj_gs2_dialogs.c - W3 contract stubs (lj_gs2_dialogs.h): open logs and returns NULL until the gs2 track lands the behaviour. */
#include <stddef.h>
#include "windows/lj_gs2_dialogs.h"
#include "core/lj_log.h"

lj_window *lj_gs2_import_open(void) { lj_log("lj_win_open: gs2_import has no behaviour file yet (W3 gs2)"); return NULL; }
void       lj_gs2_import_close(void) { }
lj_window *lj_gs2_import_get(void) { return NULL; }
void       lj_gs2_import_refresh(void) { }
lj_window *lj_gs2_export_open(void) { lj_log("lj_win_open: gs2_export has no behaviour file yet (W3 gs2)"); return NULL; }
void       lj_gs2_export_close(void) { }
lj_window *lj_gs2_export_get(void) { return NULL; }
void       lj_gs2_export_refresh(void) { }
void       lj_gs2_dialogs_bind_fns(void) { }
