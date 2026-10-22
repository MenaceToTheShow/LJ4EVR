/* lj_about_form.c - see lj_about_form.h. TABOUTFORM2 handlers: FormCreate FormDestroy FormKeyPress
 * Label4Click Timer1Timer (ME-16). Timer1 scrolled Martin's credits; with our five lines there is
 * nothing to scroll, so it is bound and idle. */
#include "windows/lj_about_form.h"

#include <stdio.h>
#include "core/lj_log.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "version.h"

static struct { lj_window *win; int close_pending; } ab;

static const char *const lj_about_lines[] = {
    "LJ4EVR " LJ_VERSION_STR,
    "A clone of Martin LightJockey 2 with sACN output.",
    "",
    "Third-party notices: " LJ_ABOUT_NOTICES,
    "LightJockey is a trademark of Martin Professional; LJ4EVR is not affiliated with Martin.",   /* docs/provenance.md trademark line (ME-37) */
};

void lj_win_about_text(char *dst, size_t cap) { if (dst && cap) snprintf(dst, cap, "LJ4EVR %s\nThird-party notices: %s", LJ_VERSION_STR, LJ_ABOUT_NOTICES); }

static void on_key(lj_widget *w, void *ud) { (void)w; (void)ud; ab.close_pending = 1; }
static void on_link(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("about: %s is beside the program file", LJ_ABOUT_NOTICES); }   /* ponytail: no shell-open from windows/; the app can open it when asked */
static const lj_binding lj_ab_bindings[] = {
    { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "FormKeyPress", on_key, NULL }, { "Label4Click", on_link, NULL }, { "Timer1Timer", lj_handler_noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_about_form_open(void)
{
    if (ab.win) return ab.win;
    ab.win = lj_window_create(&lj_form_TABOUTFORM2, lj_ab_bindings);
    if (!ab.win) { lj_log("about: cannot create TAboutForm2"); return NULL; }
    ab.close_pending = 0;
    lj_w_set_caption(lj_window_find(ab.win, "Label1"), "(C) 2026 LJ4EVR contributors, MIT licence");
    lj_w_set_caption(lj_window_find(ab.win, "Label2"), "LJ4EVR " LJ_VERSION_STR);
    lj_w_set_caption(lj_window_find(ab.win, "Label3"), "Third-party notices:");
    lj_w_set_caption(lj_window_find(ab.win, "Label4"), LJ_ABOUT_NOTICES);
    lj_w_memo_set_lines(lj_window_find(ab.win, "RichEdit1"), lj_about_lines, LJ_COUNT_OF(lj_about_lines));
    lj_win_show(ab.win, 0);
    return ab.win;
}

void lj_about_form_close(void)
{
    if (!ab.win) return;
    lj_win_hide(ab.win);
    lj_window_destroy(ab.win);
    ab.win = NULL; ab.close_pending = 0;
}

lj_window *lj_about_form_window(void) { return ab.win; }
void lj_about_form_refresh(void) { if (ab.close_pending) lj_about_form_close(); }
