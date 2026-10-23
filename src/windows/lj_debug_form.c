/* lj_debug_form.c - the DEBUG family (W4-DBG, design 13 item 10, ME-28): TDebugForm, TStaticsDebugForm, TMsgDebugForm and
 * TMemoryViewForm "System Memory". No CHM GIF exists for any of them (undocumented, note 13 s1.2): DFM numbers only.
 *
 * TDebugForm: View > De&bugWindow is hidden in the DFM; Main.Ini [DEBUG] DebugWindow=1 unhides it (PROPOSAL, the same
 * section as the DMX monitor's MonitorLayer). The DFM's labels stay verbatim; the counters are the app's integer view
 * copy (seam 3: tick_no, tick microseconds, command-array drops (design 2.3 rule 1), overruns, sACN packets/s) and the
 * frame's layer_of[] (ME-28) in the Misc tab. "Cue evaluation" and "Macro evaluation" have no engine counter: they read
 * n/a (fidelity row) until the integrator adds one. The PCI / ISA / ADP / USB Debug, Audio, 2532 and Hardware detect tabs
 * are Martin hardware (and OS probes) we do not have: their contents are disabled (a disabled TTabSheet, as the VCL
 * greys it, tab still selectable). The CLUBLITE_EXTERNALMAP shared-memory link is not reproduced (design 13 item 10), so
 * the WM_CPOPYDATA DEBUG monitor (sic) opens empty. No float anywhere: percentages are tenths in integers. */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf under /W4 /WX on MSVC */
#include "windows/lj_debug_form.h"

#include <stdio.h>
#include <string.h>
#include "core/lj_clock.h"
#include "core/lj_ini.h"
#include "core/lj_lock.h"
#include "core/lj_locale.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_statusbar.h"
#include "model/lj_statics.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"

#define LJ_DBG_STAY_ON_TOP 2u   /* = LJ_WF_STAY_ON_TOP (platform/lj_platform.h, not includable here) */
#define LJ_DBG_NA "n/a"
enum { LJ_DBG_SAMPLES = 128, LJ_DBG_MISC_LINES = LJ_CHANS / 16, LJ_DBG_MEM_ROWS = 16 };

typedef struct { uint64_t at; uint32_t us; } lj_dbg_sample;
static struct {
    lj_window *win, *statics, *msg, *memwin;
    int pending;                                   /* bit per id (LJ_WIN_DEBUG .. LJ_WIN_MEMORY_VIEW) to close at the next refresh */
    lj_dbg_sample samp[LJ_DBG_SAMPLES]; int nsamp, head;
    uint8_t layer[LJ_CHANS];
    char misc[LJ_DBG_MISC_LINES][24]; const char *misc_line[LJ_DBG_MISC_LINES];
    char act[LJ_STATIC_RUNNERS][40]; const char *act_row[LJ_STATIC_RUNNERS];
    char mem[LJ_DBG_MEM_ROWS][96]; const char *mem_row[LJ_DBG_MEM_ROWS];
    uint64_t mem_avail, mem_total; int mem_known;
} dbg;

static lj_widget *find(lj_window *w, const char *n) { return w ? lj_window_find(w, n) : NULL; }
static void set_label(lj_window *win, const char *label, const char *t)   /* lj_window_set_text (ME-129) plus TLabel AutoSize: the width follows the text */
{
    lj_widget *w = find(win, label);
    if (!w || lj_window_set_text(win, label, t) != 0) return;
    w->w = lj_text_width(lj_w_font_of(w), t);
    lj_widget_invalidate(w->parent ? w->parent : w);
}
static void disable_tree(lj_widget *w) { lj_widget *c; if (!w) return; w->enabled = 0; for (c = w->first; c; c = c->next) disable_tree(c); }

/* tenths -> "12,5" with the host decimal separator (D6) */
static void tenths(char *dst, size_t cap, uint32_t t) { snprintf(dst, cap, "%u%c%u", (unsigned)(t / 10), lj_locale_decimal(), (unsigned)(t % 10)); }

uint32_t lj_debug_form_load_tenths(uint32_t avg_tick_us, uint32_t period_us) { return period_us ? (uint32_t)((uint64_t)avg_tick_us * 1000u / period_us) : 0; }

static uint32_t avg_since(uint64_t now, uint64_t window_us)
{
    uint64_t sum = 0;
    int i, n = 0;
    for (i = 0; i < dbg.nsamp; i++) if (now - dbg.samp[i].at <= window_us) { sum += dbg.samp[i].us; n++; }
    return n ? (uint32_t)(sum / (uint64_t)n) : 0;
}

/* ---- TDebugForm ---- */
static void debug_fill(void)
{
    lj_ctx *ctx = lj_ctx_get();
    const lj_engine_status *v = &ctx->view;
    uint32_t period = ctx->engine ? lj_engine_tick_us(ctx->engine) : 0;
    uint64_t now = lj_clock_us();
    char t[96];
    int i, n = 0;
    if (!dbg.win) return;
    snprintf(t, sizeof t, "Tick: %llu", (unsigned long long)v->tick_no); set_label(dbg.win, "Label51", t);   /* PROPOSAL texts: LJ2's Label51.. are design-time placeholders */
    snprintf(t, sizeof t, "Tick time: %u us (max %u us)", (unsigned)v->tick_us_last, (unsigned)v->tick_us_max); set_label(dbg.win, "Label52", t);   /* design 3.1 */
    snprintf(t, sizeof t, "Command drops: %u", (unsigned)v->drops); set_label(dbg.win, "Label53", t);         /* design 2.3 rule 1 */
    snprintf(t, sizeof t, "Overruns: %u", (unsigned)v->overruns); set_label(dbg.win, "Label54", t);
    snprintf(t, sizeof t, "Tick period: %u us", (unsigned)period); set_label(dbg.win, "Label65", t);
    set_label(dbg.win, "Label66", "Debug on");                                                                  /* exe 0x222AD8 */
    set_label(dbg.win, "Label67", "");
    for (i = 0; i < LJ_LINKS; i++) { char nm[16]; snprintf(nm, sizeof nm, "Label%d", 68 + i); snprintf(t, sizeof t, "Link %d: %u packets/s", i + 1, (unsigned)v->sacn_pps[i]); set_label(dbg.win, nm, t); }

    dbg.samp[dbg.head].at = now; dbg.samp[dbg.head].us = v->tick_us_last;
    dbg.head = (dbg.head + 1) % LJ_DBG_SAMPLES; if (dbg.nsamp < LJ_DBG_SAMPLES) dbg.nsamp++;
    if (lj_w_speedbutton_down(find(dbg.win, "sbEnablePerformanceCounters"))) {
        tenths(t, sizeof t, lj_debug_form_load_tenths(avg_since(now, 1000000u), period)); set_label(dbg.win, "lpfCounter1", t);
        set_label(dbg.win, "lpfCounter2", LJ_DBG_NA);                                                         /* no cue-evaluation counter (the cue list runs on the UI thread) */
        snprintf(t, sizeof t, "%u%c%03u", (unsigned)(v->tick_us_last / 1000), lj_locale_decimal(), (unsigned)(v->tick_us_last % 1000)); set_label(dbg.win, "lpfCounter3", t);
        tenths(t, sizeof t, lj_debug_form_load_tenths(avg_since(now, 5000000u), period)); set_label(dbg.win, "lpfCounter4", t);
        set_label(dbg.win, "lpfCounter5", LJ_DBG_NA);                                                         /* no macro-evaluation counter */
    } else {
        static const char *const pf[] = { "lpfCounter1", "lpfCounter2", "lpfCounter3", "lpfCounter4", "lpfCounter5" };
        for (i = 0; i < 5; i++) set_label(dbg.win, pf[i], "");
    }

    if (ctx->engine) { const lj_frame *fr; lj_lock(); fr = lj_engine_frame(ctx->engine); if (fr) memcpy(dbg.layer, fr->layer_of, sizeof dbg.layer); lj_unlock(); }
    for (i = 0; i < LJ_CHANS; i += 16) {                                       /* ME-28: one line per 16 channels with a non-zero winning layer, rank digits 0..7 */
        int k, any = 0;
        for (k = 0; k < 16; k++) any |= dbg.layer[i + k];
        if (!any) continue;
        snprintf(dbg.misc[n], sizeof dbg.misc[n], "%d:%03d ", i / LJ_LINK_CH + 1, i % LJ_LINK_CH + 1);
        for (k = 0; k < 16; k++) dbg.misc[n][6 + k] = (char)('0' + (dbg.layer[i + k] & 7));
        dbg.misc[n][22] = 0;
        dbg.misc_line[n] = dbg.misc[n]; n++;
    }
    lj_w_memo_set_lines(find(dbg.win, "ReMisc"), dbg.misc_line, n);
    lj_widget_invalidate(find(dbg.win, "ReMisc"));
}

const char *lj_debug_form_label(const char *name) { lj_widget *w = find(dbg.win, name); const char *c = w ? lj_w_caption(w) : NULL; return c ? c : ""; }
int lj_debug_form_misc_lines(void) { lj_widget *w = find(dbg.win, "ReMisc"); return w ? ((lj_w_memo *)w)->n : 0; }
const char *lj_debug_form_misc_line(int i) { lj_widget *w = find(dbg.win, "ReMisc"); const lj_w_memo *m = (const lj_w_memo *)w; return m && i >= 0 && i < m->n ? m->lines[i] : ""; }

static void on_dbg_close(lj_widget *w, void *ud) { (void)w; (void)ud; dbg.pending |= 1 << (LJ_WIN_DEBUG - LJ_WIN_DEBUG); }
static void on_dbg_timer(lj_widget *w, void *ud) { (void)w; (void)ud; debug_fill(); }
static void on_perf(lj_widget *w, void *ud) { (void)w; (void)ud; debug_fill(); }
static void on_clr(lj_widget *w, void *ud) { (void)w; (void)ud; lj_w_memo_set_lines(find(dbg.win, "ReMisc"), NULL, 0); lj_widget_invalidate(find(dbg.win, "ReMisc")); }   /* until the next timer pass */
static const lj_binding lj_dbg_bindings[] = {
    { "FormClose", on_dbg_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "Timer1Timer", on_dbg_timer, NULL },
    { "sbEnablePerformanceCountersClick", on_perf, NULL },
    { "SpeedButton1Click", lj_handler_noop, NULL },   /* PCI Debug Stop/Go: Martin's PCI card thread (greyed) */
    { "SpeedButton2Click", on_clr, NULL },
    { "SpeedButton3Click", lj_handler_noop, NULL },   /* "--------": LJ2's log separator; our Misc tab is a live readout */
    { NULL, NULL, NULL } };

static lj_window *debug_open(void)
{
    static const char *const hw_tabs[] = { "TabSheet1", "TabSheet2", "TabSheet3", "TabSheet6", "TabSheet5", "ts2532", "TabSheet8" };   /* PCI ISA ADP USB Audio 2532 Hardware detect */
    size_t i;
    if (dbg.win) return dbg.win;
    dbg.win = lj_window_create(&lj_form_TDEBUGFORM, lj_dbg_bindings);
    if (!dbg.win) { lj_log("debug: cannot create TDebugForm"); return NULL; }
    for (i = 0; i < sizeof hw_tabs / sizeof hw_tabs[0]; i++) disable_tree(find(dbg.win, hw_tabs[i]));
    dbg.nsamp = dbg.head = 0;
    debug_fill();
    lj_win_show(dbg.win, LJ_DBG_STAY_ON_TOP);
    return dbg.win;
}

/* ---- TStaticsDebugForm: the active static runners from the view (lvActive); lvRestore and the eval count have no source ---- */
static void statics_fill(void)
{
    const lj_engine_status *v = &lj_ctx_get()->view;
    int i, n = 0;
    if (!dbg.statics) return;
    for (i = 0; i < LJ_STATIC_RUNNERS; i++) {
        unsigned g = v->static_slot_of[i];
        if (!v->static_active[i] || g == 0xFFFFu) continue;
        snprintf(dbg.act[n], sizeof dbg.act[n], "Page %u slot %u (%u%%)", g / LJ_STATICS2_SLOTS_MAX + 1, g % LJ_STATICS2_SLOTS_MAX + 1, (unsigned)v->static_level[i]);   /* PROPOSAL text */
        dbg.act_row[n] = dbg.act[n]; n++;
    }
    lj_w_listview_set_items(find(dbg.statics, "lvActive"), dbg.act_row, n);
    lj_widget_invalidate(find(dbg.statics, "lvActive"));
    set_label(dbg.statics, "lUpdateEval", "Evals: " LJ_DBG_NA);   /* exe 'Evals: %d' 0x211DEC: no statics-evaluation counter */
}
static const lj_binding lj_sdbg_bindings[] = { { "FormCreate", lj_handler_noop, NULL }, { "FormDestroy", lj_handler_noop, NULL }, { "lvData", lj_handler_noop, NULL },   /* OwnerData: rows are set, not fetched */
    { NULL, NULL, NULL } };

/* ---- TMsgDebugForm "WM_CPOPYDATA DEBUG": no external-link interface, so no message ever arrives ---- */
static const lj_binding lj_mdbg_bindings[] = { { NULL, NULL, NULL } };

/* The two forms with no OnClose in their DFM: the root's close event is caught here (as lj_app.c wraps the main window), the
 * window closes at the next refresh. */
static struct { lj_widget *root; void (*fn)(lj_widget *, int, void *); void *ud; } lj_dbg_wrap[2];
static void wrap_event(lj_widget *w, int ev, void *ud)
{
    int i;
    for (i = 0; i < 2; i++) if (lj_dbg_wrap[i].root == w) {
        if (ev == LJ_WEV_CLOSE) { dbg.pending |= 1 << (i == 0 ? LJ_WIN_STATICS_DEBUG - LJ_WIN_DEBUG : LJ_WIN_MSG_DEBUG - LJ_WIN_DEBUG); return; }
        if (lj_dbg_wrap[i].fn) lj_dbg_wrap[i].fn(w, ev, lj_dbg_wrap[i].ud);
        return;
    }
    (void)ud;
}
static void wrap_close(lj_window *win, int i)
{
    lj_widget *r = lj_window_root(win);
    lj_dbg_wrap[i].root = r; lj_dbg_wrap[i].fn = r->on_event; lj_dbg_wrap[i].ud = r->ud;
    r->on_event = wrap_event;
}

/* ---- TMemoryViewForm "System Memory" (View > Windows System Memory, [01 s4]) ---- */
void lj_debug_form_set_memory(uint64_t avail_bytes, uint64_t total_bytes) { dbg.mem_avail = avail_bytes; dbg.mem_total = total_bytes; dbg.mem_known = total_bytes != 0; }

static void memory_fill(void)
{
    lj_w_listview *lv = (lj_w_listview *)find(dbg.memwin, "LV");
    int i, n;
    if (!lv) return;
    n = lv->n < LJ_DBG_MEM_ROWS ? lv->n : LJ_DBG_MEM_ROWS;
    for (i = 0; i < n; i++) {
        const char *src = lv->items[i], *tab = strchr(src, '\t');
        char type[48], val[40];
        size_t tl = tab ? (size_t)(tab - src) : strlen(src);
        if (tl >= sizeof type) tl = sizeof type - 1;
        memcpy(type, src, tl); type[tl] = 0;                                  /* the DFM's 16 row names verbatim (Martin's "Uncomitted" kept) */
        lj_str_copy(val, sizeof val, LJ_DBG_NA);                              /* rows 0-8 are Delphi's heap manager (GetHeapStatus); page file / virtual need an OS query */
        if (strcmp(type, "MemoryLoad") == 0) {
            const char *pct = lj_statusbar_panel_text(LJ_SB_MEMORY);          /* the gauge's "79%" = free; load = 100 - free */
            int freep;
            if (dbg.mem_known) snprintf(val, sizeof val, "%u %%", (unsigned)(100u - (unsigned)(dbg.mem_avail * 100u / dbg.mem_total)));
            else if (pct && sscanf(pct, "%d", &freep) == 1 && freep >= 0 && freep <= 100) snprintf(val, sizeof val, "%d %%", 100 - freep);
        } else if (dbg.mem_known && strcmp(type, "Total Physical") == 0) snprintf(val, sizeof val, "%llu KB", (unsigned long long)(dbg.mem_total >> 10));
        else if (dbg.mem_known && strcmp(type, "Available Physical") == 0) snprintf(val, sizeof val, "%llu KB", (unsigned long long)(dbg.mem_avail >> 10));
        snprintf(dbg.mem[i], sizeof dbg.mem[i], "%s\t%s\t", type, val);
        dbg.mem_row[i] = dbg.mem[i];
    }
    lj_w_listview_set_items(&lv->base, dbg.mem_row, n);
    lj_widget_invalidate(&lv->base);
}
static void on_mem_close(lj_widget *w, void *ud) { (void)w; (void)ud; dbg.pending |= 1 << (LJ_WIN_MEMORY_VIEW - LJ_WIN_DEBUG); }
static void on_mem_timer(lj_widget *w, void *ud) { (void)w; (void)ud; memory_fill(); }
static const lj_binding lj_mem_bindings[] = { { "FormClose", on_mem_close, NULL }, { "FormCreate", lj_handler_noop, NULL }, { "FormShow", lj_handler_noop, NULL },
    { "LVClick", lj_handler_noop, NULL }, { "Timer1Timer", on_mem_timer, NULL }, { NULL, NULL, NULL } };

/* ---- the family ---- */
static lj_window **slot(enum lj_win id)
{
    switch (id) {
    case LJ_WIN_DEBUG: return &dbg.win;
    case LJ_WIN_STATICS_DEBUG: return &dbg.statics;
    case LJ_WIN_MSG_DEBUG: return &dbg.msg;
    case LJ_WIN_MEMORY_VIEW: return &dbg.memwin;
    default: return NULL;
    }
}

lj_window *lj_debug_form_open(enum lj_win id)
{
    lj_window **s = slot(id);
    if (!s) return NULL;
    if (*s) return *s;
    if (id == LJ_WIN_DEBUG) return debug_open();
    *s = lj_window_create(id == LJ_WIN_STATICS_DEBUG ? &lj_form_TSTATICSDEBUGFORM : id == LJ_WIN_MSG_DEBUG ? &lj_form_TMSGDEBUGFORM : &lj_form_TMEMORYVIEWFORM,
                          id == LJ_WIN_STATICS_DEBUG ? lj_sdbg_bindings : id == LJ_WIN_MSG_DEBUG ? lj_mdbg_bindings : lj_mem_bindings);
    if (!*s) { lj_log("debug: cannot create %s", lj_win_class(id)); return NULL; }
    if (id == LJ_WIN_STATICS_DEBUG) { wrap_close(*s, 0); statics_fill(); }
    if (id == LJ_WIN_MSG_DEBUG) wrap_close(*s, 1);
    if (id == LJ_WIN_MEMORY_VIEW) memory_fill();
    lj_win_show(*s, LJ_DBG_STAY_ON_TOP);                                      /* all four are fsStayOnTop */
    return *s;
}

void lj_debug_form_close(enum lj_win id)
{
    lj_window **s = slot(id);
    if (!s || !*s) return;
    lj_win_hide(*s);
    lj_window_destroy(*s);
    *s = NULL;
    dbg.pending &= ~(1 << (id - LJ_WIN_DEBUG));
}

lj_window *lj_debug_form_get(enum lj_win id) { lj_window **s = slot(id); return s ? *s : NULL; }

void lj_debug_form_refresh(enum lj_win id)
{
    if (!slot(id)) return;
    if (dbg.pending & (1 << (id - LJ_WIN_DEBUG))) { lj_debug_form_close(id); return; }
    if (id == LJ_WIN_DEBUG) debug_fill();
    else if (id == LJ_WIN_STATICS_DEBUG) statics_fill();
    else if (id == LJ_WIN_MEMORY_VIEW && dbg.memwin) memory_fill();
}

void lj_debug_form_bind_fns(void)   /* the unhide (PROPOSAL): Main.Ini [DEBUG] DebugWindow=1 shows View > De&bugWindow */
{
    lj_ctx *ctx = lj_ctx_get();
    lj_window *main = lj_desktop_window();
    if (main && lj_ini_geti(ctx->main_ini, "DEBUG", "DebugWindow", 0)) lj_menu_set_visible(lj_window_menus(main), "M_ViewDebug", 1);
}
