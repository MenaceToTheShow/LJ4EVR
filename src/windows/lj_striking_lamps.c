/* lj_striking_lamps.c - see lj_striking_lamps.h. TAUTOLAMPFORM handlers: FormClose FormCreate
 * bCloseClick Timer1Timer (ME-16). */
#include "windows/lj_striking_lamps.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "fixlib/lj_profile.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"

static struct {
    lj_window *win; int stay_open, close_pending;
    int n, done; int fx[LJ_FIXTURES]; char line[LJ_FIXTURES][96]; const char *items[LJ_FIXTURES];
    uint32_t interval_ms, next_ms; int started;
    void (*strike)(int fixture, void *ud); void *ud;
} sl;

static const lj_profile *profile_of(int i) { lj_ctx *c = lj_ctx_get(); return c->show && c->show->fixture[i].link ? lj_fixlib_by_id(c->show->fixture[i].profile_id) : NULL; }

/* the default strike: the profile's Lamp On values into the current scene, Snap (the Lamp control's own path, "stored in the current scene") */
static void scene_strike(int i, void *ud)
{
    lj_ctx *c = lj_ctx_get();
    const lj_profile *p = profile_of(i);
    int base = c->show ? lj_show_fixture_base(c->show, i) : -1, o;
    (void)ud;
    if (!p || base < 0 || !c->show->current_seq || c->current_scene >= c->show->current_seq->n) return;
    lj_lock();
    for (o = 0; o < p->footprint && o < LJ_PROFILE_CH && base + o < LJ_CHANS; o++)
        if (p->lamp_on.defined[o]) { lj_scene *s = &c->show->current_seq->scenes[c->current_scene]; s->val[base + o] = p->lamp_on.value[o]; s->state[base + o] = LJ_SNAP; }
    lj_unlock();
    lj_ctx_set_modified(LJ_MOD_SEQ, 1);
}

static void show_rows(void)
{
    lj_widget *lv = sl.win ? lj_window_find(sl.win, "LV") : NULL, *pg = sl.win ? lj_window_find(sl.win, "pg1") : NULL;
    int i;
    for (i = 0; i < sl.n; i++) {
        const lj_profile *p = profile_of(sl.fx[i]);
        snprintf(sl.line[i], sizeof sl.line[i], "%d\t%s\t%s", sl.fx[i] + 1, p ? p->name : "", i < sl.done ? "Lamp On" : "");   /* Action text PROPOSAL rig */
        sl.items[i] = sl.line[i];
    }
    if (lv) lj_w_listview_set_items(lv, sl.items, sl.n);
    if (pg) lj_w_progressbar_set(pg, sl.n ? sl.done * 100 / sl.n : 0);   /* TProgressBar Max 100 (DFM default) */
}

static lj_window *create_win(void);

int lj_striking_lamps_start(int stay_open, uint32_t interval_ms)
{
    lj_ctx *c = lj_ctx_get();
    int i, any = lj_sel_count() > 0;
    sl.n = sl.done = 0;
    for (i = 0; i < LJ_FIXTURES && c->show; i++) {
        const lj_profile *p = profile_of(i);
        if (!p || !p->lamp_on.enabled || (any && !lj_sel_is_selected(i))) continue;
        sl.fx[sl.n++] = i;
    }
    if (sl.n == 0) lj_log("striking lamps: no fixture with a Lamp On function");
    if (sl.n == 0 && !stay_open) return 0;                                 /* the startup run has nothing to show; the menu run opens empty */
    sl.stay_open = stay_open; sl.interval_ms = interval_ms ? interval_ms : LJ_STRIKE_INTERVAL_MS; sl.started = 0;
    if (!create_win()) return 0;
    show_rows();
    return sl.n;
}

int lj_striking_lamps_step(uint32_t now_ms)
{
    if (!sl.win || sl.done >= sl.n) return sl.done;
    if (sl.started && (int32_t)(now_ms - sl.next_ms) < 0) return sl.done;
    (sl.strike ? sl.strike : scene_strike)(sl.fx[sl.done], sl.ud);
    sl.done++; sl.started = 1; sl.next_ms = now_ms + sl.interval_ms;
    show_rows();
    if (sl.done >= sl.n && !sl.stay_open) sl.close_pending = 1;
    return sl.done;
}

int  lj_striking_lamps_count(void) { return sl.n; }
int  lj_striking_lamps_fixture(int i) { return i >= 0 && i < sl.n ? sl.fx[i] : -1; }
void lj_striking_lamps_set_strike(void (*fn)(int fixture, void *ud), void *ud) { sl.strike = fn; sl.ud = ud; }

static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void on_stop(lj_widget *w, void *ud) { (void)w; (void)ud; sl.close_pending = 1; }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_striking_lamps_step((uint32_t)(lj_clock_us() / 1000)); }
static const lj_binding lj_sl_bindings[] = {
    { "FormClose", on_stop, NULL }, { "FormCreate", noop, NULL }, { "bCloseClick", on_stop, NULL }, { "Timer1Timer", on_timer, NULL },
    { NULL, NULL, NULL } };

static lj_window *create_win(void)
{
    if (sl.win) return sl.win;
    sl.win = lj_window_create(&lj_form_TAUTOLAMPFORM, lj_sl_bindings);
    if (!sl.win) { lj_log("striking lamps: cannot create TAutoLampForm"); return NULL; }
    sl.close_pending = 0;
    lj_win_show(sl.win, 0);
    return sl.win;
}

void lj_striking_lamps_close(void)
{
    if (!sl.win) return;
    lj_win_hide(sl.win);
    lj_window_destroy(sl.win);
    sl.win = NULL; sl.close_pending = 0; sl.n = sl.done = 0;
}

lj_window *lj_striking_lamps_open(void)                                    /* System > Auto Strike Lamps (LJ_ACT_STRIKING_LAMPS) */
{
    if (sl.win) return sl.win;
    lj_striking_lamps_start(1, LJ_STRIKE_INTERVAL_MS);
    return sl.win;
}

lj_window *lj_striking_lamps_window(void) { return sl.win; }
void lj_striking_lamps_refresh(void)
{
    if (sl.win && !sl.close_pending) lj_striking_lamps_step((uint32_t)(lj_clock_us() / 1000));
    if (sl.close_pending) lj_striking_lamps_close();
}
