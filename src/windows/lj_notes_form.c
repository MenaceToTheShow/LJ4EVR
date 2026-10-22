/* lj_notes_form.c - see lj_notes_form.h. TNOTEFORM handlers: FormClose FormCreate FormDestroy
 * FormShow Memo1Change (ME-16). */
#include "windows/lj_notes_form.h"

#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"

/* ---- memo editor ---- */

enum { LJ_ME_MAX = 4 };                                        /* Notes + the Backup notes box */
static lj_memo_edit *lj_me_reg[LJ_ME_MAX];
static lj_widget_vt lj_me_vt;
static const lj_widget_vt *lj_me_base;

static void me_show(lj_memo_edit *e)
{
    int n = 0;
    char *p = e->split;
    memcpy(e->split, e->text, sizeof e->split);
    e->line[n++] = p;
    for (; *p && n < LJ_MEMO_EDIT_LINES; p++) if (*p == '\n') { *p = 0; if (p > e->split && p[-1] == '\r') p[-1] = 0; e->line[n++] = p + 1; }
    if (e->memo) { lj_w_memo_set_lines(e->memo, e->line, n); lj_widget_invalidate(e->memo); }
}

static int me_key(lj_widget *w, const lj_key_ev *k)
{
    lj_memo_edit *e = NULL;
    size_t len;
    int i;
    for (i = 0; i < LJ_ME_MAX; i++) if (lj_me_reg[i] && lj_me_reg[i]->memo == w) e = lj_me_reg[i];
    if (!e || !w->enabled) return lj_me_base->key(w, k);
    len = strlen(e->text);
    if (k->type == LJ_EV_CHAR && k->ch >= 32) { if (len + 1 >= sizeof e->text) return 1; e->text[len] = (char)k->ch; e->text[len + 1] = 0; }
    else if (k->type == LJ_EV_KEY_DOWN && k->key == 0x0D) { if (len + 2 >= sizeof e->text) return 1; memcpy(e->text + len, "\r\n", 3); }   /* VK_RETURN; CRLF as a TMemo stores it */
    else if (k->type == LJ_EV_KEY_DOWN && k->key == LJ_VK_BACK) { if (!len) return 1; len -= (len >= 2 && e->text[len - 1] == '\n' && e->text[len - 2] == '\r') ? 2 : 1; e->text[len] = 0; }
    else return lj_me_base->key(w, k);
    e->changed = 1;
    me_show(e);
    lj_w_emit(w, LJ_WEV_CHANGE);
    return 1;
}

void lj_memo_edit_attach(lj_memo_edit *e, lj_widget *memo)
{
    int i;
    if (!e || !memo) return;
    e->memo = memo;
    if (!lj_me_base) { lj_me_base = memo->vt; lj_me_vt = *memo->vt; lj_me_vt.key = me_key; }
    memo->vt = &lj_me_vt;
    for (i = 0; i < LJ_ME_MAX; i++) if (!lj_me_reg[i] || lj_me_reg[i] == e) { lj_me_reg[i] = e; break; }
    me_show(e);
}

void lj_memo_edit_detach(lj_memo_edit *e)
{
    int i;
    for (i = 0; i < LJ_ME_MAX; i++) if (lj_me_reg[i] == e) lj_me_reg[i] = NULL;
    if (e) e->memo = NULL;
}

int lj_memo_edit_set(lj_memo_edit *e, const char *text)
{
    if (!e || !text || strlen(text) >= sizeof e->text) return -1;
    lj_str_copy(e->text, sizeof e->text, text);
    me_show(e);
    return 0;
}

/* ---- the Notes window ---- */

static struct { lj_window *win; lj_memo_edit ed; int close_pending; } nf;

static int notes_path(char *out, size_t cap) { lj_ctx *c = lj_ctx_get(); return c->lib ? lj_library_path(c->lib, LJ_NOTES_FILE, out, cap) : -1; }

static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void on_close(lj_widget *w, void *ud) { (void)w; (void)ud; nf.close_pending = 1; }
static const lj_binding lj_nf_bindings[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormShow", noop, NULL }, { "Memo1Change", noop, NULL },
    { NULL, NULL, NULL } };

lj_window *lj_notes_form_open(void)
{
    char path[LJ_PATH_CAP];
    if (nf.win) return nf.win;
    nf.win = lj_window_create(&lj_form_TNOTEFORM, lj_nf_bindings);
    if (!nf.win) { lj_log("notes: cannot create TNoteForm"); return NULL; }
    nf.ed.text[0] = 0; nf.ed.changed = 0; nf.close_pending = 0;
    if (notes_path(path, sizeof path) == 0 && lj_notes_load(path, nf.ed.text, sizeof nf.ed.text) != 0) lj_log("notes: %s not read", path);   /* the DFM's "Memo1" placeholder never shows */
    lj_memo_edit_attach(&nf.ed, lj_window_find(nf.win, "Memo1"));
    lj_win_show(nf.win, 0);
    return nf.win;
}

void lj_notes_form_close(void)
{
    char path[LJ_PATH_CAP];
    if (!nf.win) return;
    if (nf.ed.changed && notes_path(path, sizeof path) == 0) lj_notes_save(path, nf.ed.text);
    lj_memo_edit_detach(&nf.ed);
    lj_win_hide(nf.win);
    lj_window_destroy(nf.win);
    nf.win = NULL; nf.close_pending = 0;
}

lj_window *lj_notes_form_window(void) { return nf.win; }
void lj_notes_form_refresh(void) { if (nf.close_pending) lj_notes_form_close(); }
const char *lj_notes_form_text(void) { return nf.win ? nf.ed.text : ""; }
int lj_notes_form_set_text(const char *text)
{
    if (!nf.win || lj_memo_edit_set(&nf.ed, text) != 0) return -1;
    nf.ed.changed = 1;
    return 0;
}
