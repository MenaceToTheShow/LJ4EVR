/* lj_global_patch.c - see lj_global_patch.h. TGLOBALPATCHFORM bound (ME-16: every OnXxx of its DFM). */
#include "windows/lj_global_patch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "model/lj_patch.h"
#include "model/lj_show.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/lj_menu.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "ui/widgets/lj_w_checkbox.h"
#include "ui/widgets/lj_w_edit.h"
#include "ui/widgets/lj_w_radiobutton.h"
#include "windows/lj_warnings.h"

#define LJ_GP_ROW 192

typedef struct {
    lj_window *win;
    lj_patch_ch work[LJ_CHANS];
    char row[LJ_CHANS][LJ_GP_ROW]; const char *rows[LJ_CHANS];
    char counts[6][32];
    int sel, filling, changed, htp_changed;
    uint8_t value[LJ_CHANS];
} lj_gp_state;
static lj_gp_state *gp;   /* ~260 KB: heap while open */
static void (*lj_gp_post)(const lj_cmd *cmd);

static const char *const lj_gp_mode_text[] = { "Abs", "LoTP", "HTP", ">0", "<255" };   /* In mode/Sw column, short forms of the DMX in Mode radios (PROPOSAL rig: globalpatch.gif is reduced) */
static const char *const lj_gp_htp_items[LJ_HTP_GROUPS] = {
    "HTP Group 1", "HTP Group 2", "HTP Group 3", "HTP Group 4", "HTP Group 5", "HTP Group 6", "HTP Group 7", "HTP Group 8",
    "HTP Group 9", "HTP Group 10", "HTP Group 11", "HTP Group 12", "HTP Group 13", "HTP Group 14", "HTP Group 15", "HTP Group 16",
    "HTP Group 17", "HTP Group 18", "HTP Group 19", "HTP Group 20", "HTP Group 21", "HTP Group 22", "HTP Group 23", "HTP Group 24" };   /* PROPOSAL rig: clbGroups item text (no GIF shows it); ctx->htp_names wins when set */

lj_window *lj_global_patch_window(void) { return gp ? gp->win : NULL; }
const lj_patch_ch *lj_global_patch_work(void) { return gp ? gp->work : NULL; }
void lj_global_patch_set_post(void (*post)(const lj_cmd *cmd)) { lj_gp_post = post; }
static lj_widget *find(const char *n) { return gp && gp->win ? lj_window_find(gp->win, n) : NULL; }
static void post(const lj_cmd *c) { if (lj_gp_post) lj_gp_post(c); else lj_ctx_post(c); }

int lj_global_patch_htp_masters(void) { lj_ctx *ctx = lj_ctx_get(); return ctx->main_ini ? lj_ini_geti(ctx->main_ini, "GLOBALPATCH", "HTPMasters", 0) != 0 : 0; }   /* PROPOSAL rig: LJ2's key is not in the shipped Main.Ini */

static void addr_text(int code, char *out, size_t cap) { snprintf(out, cap, "%d:%03d", code / LJ_LINK_CH + 1, code % LJ_LINK_CH + 1); }

static void row_text(int c)
{
    const lj_show *sh = lj_ctx_get()->show;
    const lj_patch_ch *p = &gp->work[c];
    char adr[24], fx[48] = "", desc[16] = "", out[12] = "", in[8] = "", mode[12] = "", htp[72] = "", mn[4] = "", mx[4] = "";
    int f = sh ? lj_show_fixture_at(sh, c) : -1, k, n = 0;
    addr_text(c, adr, sizeof adr);
    if (f >= 0) {
        snprintf(fx, sizeof fx, "(%d) %s", f + 1, sh->fixture[f].user_name);
        snprintf(desc, sizeof desc, "Ch %d", c - lj_show_fixture_base(sh, f) + 1);   /* PROPOSAL rig: LJ2 shows the control function here */
    }
    if (p->src_kind == LJ_SRC_DMX_OUT) addr_text(lj_addr_code(p->src_link, p->src_ch), out, sizeof out);
    if (p->src_kind == LJ_SRC_DMX_IN) { snprintf(in, sizeof in, "%u", (unsigned)p->src_ch); lj_str_copy(mode, sizeof mode, lj_gp_mode_text[p->in_mode < 5 ? p->in_mode : 0]); }
    for (k = 0; k < LJ_HTP_GROUPS && n < (int)sizeof htp - 4; k++) if (p->htp_mask & (1u << k)) n += snprintf(htp + n, sizeof htp - (size_t)n, n ? ",%d" : "%d", k + 1);
    if (p->min) snprintf(mn, sizeof mn, "%u", p->min);           /* 0/255 defaults not shown (02 s7.1) */
    if (p->max != 255) snprintf(mx, sizeof mx, "%u", p->max);
    if (snprintf(gp->row[c], LJ_GP_ROW, "%s\t%s\t%s\t%u\t%s\t%s\t%s\t%s\t%s\t%s\t%s", adr, fx, desc, gp->value[c], out, in, mode, htp, p->invert ? "Yes" : "", mn, mx) < 0) gp->row[c][0] = 0;
    gp->rows[c] = gp->row[c];
}

static void counts(void)
{
    static const char *const lbl[6] = { "lPatchedOut", "lPatchedIn", "lInverted", "lMin", "lMax", "lHTPGroups" };
    static const char *const fmt[6] = { "Source Out:%d", "Source In:%d", "Inverted:%d", "Min Limit:%d", "Max Limit:%d", "HTP Groups:%d" };   /* exe 0x1BD6CC.. formats; Min Limit 0x1BD6FC, Max Limit 0x1BD714 (ME-97) */
    int n[6] = { 0 }, c, i;
    for (c = 0; c < LJ_CHANS; c++) {
        const lj_patch_ch *p = &gp->work[c];
        n[0] += p->src_kind == LJ_SRC_DMX_OUT; n[1] += p->src_kind == LJ_SRC_DMX_IN; n[2] += p->invert != 0;
        n[3] += p->min != 0; n[4] += p->max != 255; n[5] += p->htp_mask != 0;
    }
    for (i = 0; i < 6; i++) { snprintf(gp->counts[i], sizeof gp->counts[i], fmt[i], n[i]); lj_w_set_caption(find(lbl[i]), gp->counts[i]); }
}

static void fill_list(void)
{
    lj_widget *lv = find("lv1");
    int c, keep = gp->sel;
    for (c = 0; c < LJ_CHANS; c++) row_text(c);
    if (lv) { lj_w_listview_set_items(lv, gp->rows, LJ_CHANS); ((lj_w_listview *)lv)->index = keep; lj_widget_invalidate(lv); }
    counts();
}

static void panel_fill(void)
{
    static const char *const rb[5] = { "rbAbs", "rbLTP", "rbHTP", "rb0", "rb255" };
    const lj_patch_ch *p;
    char t[12];
    int k, on = gp->sel >= 0;
    lj_widget *clb = find("clbGroups");
    gp->filling = 1;
    lj_w_set_caption(find("bSet"), "<- Set Values");
    if (find("bSet")) find("bSet")->enabled = (unsigned)on;
    if (find("bClear")) find("bClear")->enabled = (unsigned)on;
    if (!on) { gp->filling = 0; return; }
    p = &gp->work[gp->sel];
    lj_w_checkbox_set_checked(find("cbInverted"), p->invert);
    snprintf(t, sizeof t, "%u", p->min); lj_w_edit_set_text(find("eMin"), t);
    snprintf(t, sizeof t, "%u", p->max); lj_w_edit_set_text(find("eMax"), t);
    if (p->src_kind == LJ_SRC_DMX_OUT) snprintf(t, sizeof t, "%d", lj_addr_code(p->src_link, p->src_ch) + 1); else t[0] = 0;
    lj_w_edit_set_text(find("eSourceOut"), t);
    if (p->src_kind == LJ_SRC_DMX_IN) snprintf(t, sizeof t, "%u", (unsigned)p->src_ch); else t[0] = 0;
    lj_w_edit_set_text(find("eSourceIn"), t);
    for (k = 0; k < 5; k++) {
        lj_widget *r = find(rb[k]);
        if (!r) continue;
        r->enabled = p->src_kind == LJ_SRC_DMX_IN;
        if (k == p->in_mode && !lj_w_radiobutton_checked(r)) lj_w_radiobutton_click(r);
    }
    if (clb) for (k = 0; k < LJ_HTP_GROUPS; k++) lj_w_list_set_checked(clb, k, (p->htp_mask >> k) & 1u);
    gp->filling = 0;
}

int lj_global_patch_select(int ch)
{
    lj_widget *lv = find("lv1");
    if (!gp || ch < 0 || ch >= LJ_CHANS) return -1;
    gp->sel = ch;
    if (lv) { ((lj_w_listview *)lv)->index = ch; lj_widget_invalidate(lv); }
    panel_fill();
    return 0;
}

void lj_global_patch_refresh(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_widget *lv;
    int c;
    if (!gp) return;
    lv = find("lv1");
    if (lv && ((lj_w_listview *)lv)->index != gp->sel) { gp->sel = ((lj_w_listview *)lv)->index; panel_fill(); }   /* lv1 has no OnChange in the DFM: the 200 ms timer notices the row */
    if (ctx->engine) {
        const lj_frame *f;
        lj_lock();
        f = lj_engine_frame(ctx->engine);
        if (f) for (c = 0; c < LJ_CHANS; c++) gp->value[c] = f->out[c / LJ_LINK_CH][c % LJ_LINK_CH];
        lj_unlock();
        for (c = 0; c < LJ_CHANS; c++) row_text(c);
        if (lv) lj_widget_invalidate(lv);
    }
}

static int num(const char *s, int lo, int hi, int *out)
{
    char *end;
    long v;
    if (!s || !*s) { *out = -1; return 0; }        /* empty = not set */
    v = strtol(s, &end, 10);
    if (*end || v < lo || v > hi) return -1;
    *out = (int)v;
    return 0;
}

const char *lj_global_patch_set_values(void)
{
    static const char *const rb[5] = { "rbAbs", "rbLTP", "rbHTP", "rb0", "rb255" };
    lj_patch_ch *tmp, n;
    const char *msg = NULL;
    int mn = -1, mx = -1, out = -1, in = -1, k;
    if (!gp || gp->sel < 0) return "no channel selected";
    n = gp->work[gp->sel];
    if (num(lj_w_edit_text(find("eMin")), 0, 255, &mn) || num(lj_w_edit_text(find("eMax")), 0, 255, &mx)) msg = "Min and Max must be 0..255";
    else if (num(lj_w_edit_text(find("eSourceOut")), 1, LJ_CHANS, &out)) msg = "DMX Out source must be 1..2048";
    else if (num(lj_w_edit_text(find("eSourceIn")), 1, LJ_LINK_CH, &in)) msg = "DMX In source must be 1..512";
    else if (out > 0 && in > 0) msg = "a channel has one source: DMX Out or DMX In";
    if (!msg) {
        n.invert = (uint8_t)lj_w_checkbox_checked(find("cbInverted"));
        n.min = (uint8_t)(mn < 0 ? 0 : mn); n.max = (uint8_t)(mx < 0 ? 255 : mx);
        n.src_kind = LJ_SRC_NONE; n.src_link = 0; n.src_ch = 0;
        if (out > 0) { n.src_kind = LJ_SRC_DMX_OUT; n.src_link = (uint8_t)((out - 1) / LJ_LINK_CH + 1); n.src_ch = (uint16_t)((out - 1) % LJ_LINK_CH + 1); }
        if (in > 0) { n.src_kind = LJ_SRC_DMX_IN; n.src_link = 1; n.src_ch = (uint16_t)in; }   /* one DMX-in universe (05 s3) */
        for (k = 0; k < 5; k++) if (find(rb[k]) && lj_w_radiobutton_checked(find(rb[k]))) n.in_mode = (uint8_t)k;
        tmp = (lj_patch_ch *)malloc(sizeof gp->work);
        if (!tmp) lj_fatal("global patch: out of memory");
        memcpy(tmp, gp->work, sizeof gp->work);
        tmp[gp->sel] = n;
        msg = lj_patch_validate(tmp);                 /* min <= max, no self-patch, ranges */
        free(tmp);
    }
    if (msg) { lj_win_message("Global Patch", msg); return msg; }
    gp->work[gp->sel] = n;
    gp->changed = 1;
    row_text(gp->sel); counts(); panel_fill();
    return NULL;
}

int lj_global_patch_apply(void)
{
    lj_ctx *ctx = lj_ctx_get();
    lj_cmd c;
    int ch, htp = 0;
    if (!gp || !ctx->show) return -1;
    lj_lock();
    for (ch = 0; ch < LJ_CHANS; ch++) htp |= ctx->show->patch[ch].htp_mask != gp->work[ch].htp_mask;
    memcpy(ctx->show->patch, gp->work, sizeof gp->work);
    lj_unlock();
    memset(&c, 0, sizeof c);
    c.kind = LJ_CMD_GLOBAL_PATCH; c.patch = ctx->show->patch;
    post(&c);
    if (htp || gp->htp_changed) { c.kind = LJ_CMD_HTP_GROUPS; c.b = (uint32_t)lj_global_patch_htp_masters(); post(&c); }
    if (ctx->lib) {
        char path[LJ_PATH_CAP];
        if (lj_library_path(ctx->lib, LJ_PATCH_FILE, path, sizeof path) != 0 || lj_patch_save(path, ctx->show->patch) != 0) lj_log("global patch: cannot write %s", LJ_PATCH_FILE);
    }
    gp->changed = gp->htp_changed = 0;
    return 0;
}


static void clear_rows(int what, int only)   /* what: 0 all, 1 DMX out, 2 DMX in, 3 inverted, 4 min, 5 max, 6 HTP groups; only = one address code or -1 = every row */
{
    int c, from = only < 0 ? 0 : only, to = only < 0 ? LJ_CHANS : only + 1;
    for (c = from; c < to; c++) {
        lj_patch_ch *p = &gp->work[c];
        if (what == 0 || what == 6) { if (p->htp_mask) gp->htp_changed = 1; p->htp_mask = 0; }
        if (what == 0 || (what == 1 && p->src_kind == LJ_SRC_DMX_OUT) || (what == 2 && p->src_kind == LJ_SRC_DMX_IN)) { p->src_kind = LJ_SRC_NONE; p->src_link = 0; p->src_ch = 0; p->in_mode = LJ_IN_ABSOLUTE; }
        if (what == 0 || what == 3) p->invert = 0;
        if (what == 0 || what == 4) p->min = 0;
        if (what == 0 || what == 5) p->max = 255;
    }
    gp->changed = 1;
    fill_list(); panel_fill();
}

static int lj_gp_pending;   /* the Options clear waiting for its "Please Confirm.." answer */
static void confirm_clear(int yes, void *ud) { (void)ud; if (yes && gp) clear_rows(lj_gp_pending, -1); }
static void ask(const char *text, int what) { lj_gp_pending = what; lj_win_confirm("Please Confirm..", text, NULL, confirm_clear, NULL); }

int lj_global_patch_menu(const char *item)
{
    static const char *const clears[] = { "P_ClearAll", "P_ClearOut", "P_ClearIn", "P_ClearInverted", "P_ClearMin", "P_ClearMax", "P_ClearChannelGroups" };
    static const char *const modes[] = { "P_Absolute", "P_LTP", "P_HTP", "P_LOW", "P_HI" };   /* the DFM Tags 0..4 = enum lj_in_mode */
    lj_ctx *ctx = lj_ctx_get();
    char path[LJ_PATH_CAP], err[160];
    int c;
    if (!gp || !item) return -1;
    for (c = 0; c < LJ_COUNT_OF(clears); c++) if (strcmp(item, clears[c]) == 0) { if (gp->sel >= 0) clear_rows(c, gp->sel); return 0; }
    for (c = 0; c < LJ_COUNT_OF(modes); c++) if (strcmp(item, modes[c]) == 0) {
        if (gp->sel >= 0 && gp->work[gp->sel].src_kind == LJ_SRC_DMX_IN) { gp->work[gp->sel].in_mode = (uint8_t)c; gp->changed = 1; row_text(gp->sel); panel_fill(); }
        return 0;
    }
    if (strcmp(item, "NewGlobalPatch1") == 0) { lj_patch_clear(gp->work); gp->changed = gp->htp_changed = 1; fill_list(); panel_fill(); }
    else if (strcmp(item, "LoadGlobalPatch1") == 0 || strcmp(item, "SaveGlobalPatch1") == 0) {
        /* ponytail: always <library>\GlobalPatch.GPA; a file picker needs a platform hook lj_win_host does not have yet */
        if (!ctx->lib || lj_library_path(ctx->lib, LJ_PATCH_FILE, path, sizeof path) != 0) { lj_log("global patch: no user library"); return -1; }
        if (item[0] == 'S') { if (lj_patch_save(path, gp->work) != 0) lj_win_message("Global Patch", "Cannot write the Global Patch file"); }
        else if (lj_patch_load(path, gp->work, err, sizeof err) != 0) lj_win_message("Global Patch", err);
        else { gp->changed = gp->htp_changed = 1; fill_list(); panel_fill(); }
    }
    else if (strncmp(item, "PatchtoLink", 11) == 0 && item[11] >= '1' && item[11] <= '3') {   /* "Patch DMX In to Link n": link n's 512 channels take DMX in 1..512 */
        int base = (item[11] - '1') * LJ_LINK_CH;
        for (c = 0; c < LJ_LINK_CH; c++) { lj_patch_ch *p = &gp->work[base + c]; p->src_kind = LJ_SRC_DMX_IN; p->src_link = 1; p->src_ch = (uint16_t)(c + 1); }
        gp->changed = 1; fill_list(); panel_fill();
    }
    else if (strcmp(item, "ClearallDMXOut1") == 0) ask("Clear All DMX Out Patches ?", 1);     /* exe 0x1BE568 */
    else if (strcmp(item, "ClearallDMXin1") == 0) ask("Clear All DMX In Patches ?", 2);       /* exe 0x1BE5F4 */
    else if (strcmp(item, "ClearAllInverted1") == 0) clear_rows(3, -1);
    else if (strcmp(item, "ClearAllMin1") == 0) clear_rows(4, -1);
    else if (strcmp(item, "ClearAllMax1") == 0) clear_rows(5, -1);
    else if (strcmp(item, "ClearAll1") == 0) ask("Clear All Patch Information ?", 0);         /* exe 0x1BE4D8 */
    else if (strcmp(item, "M_HTPgroupsaffectedgyGM") == 0) {
        int on = !lj_global_patch_htp_masters();
        if (ctx->main_ini) lj_ini_seti(ctx->main_ini, "GLOBALPATCH", "HTPMasters", on);
        lj_menu_set_checked(lj_window_menus(gp->win), item, on);
        gp->htp_changed = 1;
    }
    else if (strcmp(item, "P_OutSeq") == 0 || strcmp(item, "P_InSeq") == 0 || strcmp(item, "P_EnableUse") == 0 || strcmp(item, "P_EnableIgnore") == 0)
        lj_log("global patch: %s: not in W2 (the sequence dialogs and the DMX-in enable switch come with DMX in)", item);   /* ponytail: DMX in is W3+ */
    else return -1;
    return 0;
}

/* ---- handlers ---- */
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void on_menu(lj_widget *w, void *ud) { const lj_menu_item_desc *it = gp ? lj_window_menu_item(gp->win) : NULL; (void)w; (void)ud; if (it) lj_global_patch_menu(it->name); }
static void on_ok(lj_widget *w, void *ud) { (void)w; (void)ud; lj_global_patch_apply(); lj_global_patch_close(); }
static void on_set(lj_widget *w, void *ud) { (void)w; (void)ud; lj_global_patch_set_values(); }
static void on_clear(lj_widget *w, void *ud) { (void)w; (void)ud; if (gp && gp->sel >= 0) clear_rows(0, gp->sel); }
static void on_timer(lj_widget *w, void *ud) { (void)w; (void)ud; lj_global_patch_refresh(); }
static void on_help(lj_widget *w, void *ud) { (void)w; (void)ud; lj_log("global patch: Help: no help file is shipped (D2)"); }
static void on_mode(lj_widget *w, void *ud)
{
    static const char *const rb[5] = { "rbAbs", "rbLTP", "rbHTP", "rb0", "rb255" };
    int k;
    (void)ud;
    if (!gp || gp->filling || gp->sel < 0 || gp->work[gp->sel].src_kind != LJ_SRC_DMX_IN) return;
    for (k = 0; k < 5; k++) if (find(rb[k]) == w) { gp->work[gp->sel].in_mode = (uint8_t)k; gp->changed = 1; row_text(gp->sel); }
}
static void on_groups(lj_widget *w, void *ud)
{
    uint32_t m = 0;
    int k;
    (void)ud;
    if (!gp || gp->filling || gp->sel < 0) return;
    for (k = 0; k < LJ_HTP_GROUPS; k++) if (lj_w_list_checked(w, k)) m |= 1u << k;
    if (m != gp->work[gp->sel].htp_mask) { gp->work[gp->sel].htp_mask = m; gp->changed = gp->htp_changed = 1; row_text(gp->sel); counts(); }
}
static void locate(int dir)
{
    int c;
    for (c = gp->sel + dir; c >= 0 && c < LJ_CHANS; c += dir) if (!lj_patch_is_default(&gp->work[c])) { lj_global_patch_select(c); return; }
}
static void on_prev(lj_widget *w, void *ud) { (void)w; (void)ud; if (gp) locate(-1); }
static void on_next(lj_widget *w, void *ud) { (void)w; (void)ud; if (gp) locate(1); }
static void close_answer(int yes, void *ud) { (void)ud; if (yes) lj_global_patch_apply(); if (gp) { gp->changed = gp->htp_changed = 0; lj_global_patch_close(); } }
static void on_close(lj_widget *w, void *ud)
{
    (void)w; (void)ud;
    if (gp && (gp->changed || gp->htp_changed)) lj_win_confirm("Please Confirm..", "Global Patch was changed", "Apply changes ?", close_answer, NULL);   /* exe 0x1BD7D8 */
    else lj_global_patch_close();
}

static const lj_binding lj_gp_bindings[] = {
    { "FormClose", on_close, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormKeyDown", noop, NULL }, { "FormResize", noop, NULL },
    { "lv1Data", noop, NULL }, { "bOkClick", on_ok, NULL }, { "bSetClick", on_set, NULL }, { "bClearClick", on_clear, NULL },
    { "eSourceInChange", noop, NULL }, { "rbModeClick", on_mode, NULL }, { "cbClick", noop, NULL }, { "clbGroupsClickCheck", on_groups, NULL },
    { "bLocatePrevClick", on_prev, NULL }, { "bLocateNextClick", on_next, NULL }, { "Timer1Timer", on_timer, NULL },
    { "NewGlobalPatch1Click", on_menu, NULL }, { "LoadGlobalPatch1Click", on_menu, NULL }, { "SaveGlobalPatch1Click", on_menu, NULL },
    { "PatchtoLink", on_menu, NULL }, { "ClearallDMXOut1Click", on_menu, NULL }, { "ClearallDMXin1Click", on_menu, NULL },
    { "ClearAllInverted1Click", on_menu, NULL }, { "ClearAllMin1Click", on_menu, NULL }, { "ClearAllMax1Click", on_menu, NULL }, { "ClearAll1Click", on_menu, NULL },
    { "PrefsClick", on_menu, NULL }, { "Help1Click", on_help, NULL }, { "PopupMenu1Popup", noop, NULL }, { "P_ClearClick", on_menu, NULL },
    { "P_OutSeqClick", on_menu, NULL }, { "P_InSeqClick", on_menu, NULL }, { "P_DMXModeClick", on_menu, NULL }, { "P_EnableIgnoreClick", on_menu, NULL },
    { NULL, NULL, NULL }
};

lj_window *lj_global_patch_open(void)
{
    static const char *items[LJ_HTP_GROUPS];
    lj_ctx *ctx = lj_ctx_get();
    lj_widget *clb;
    int k;
    if (gp) return gp->win;
    gp = (lj_gp_state *)calloc(1, sizeof *gp);
    if (!gp) lj_fatal("global patch: out of memory");
    gp->sel = -1;
    if (ctx->show) { lj_lock(); memcpy(gp->work, ctx->show->patch, sizeof gp->work); lj_unlock(); }
    else lj_patch_init(gp->work);
    gp->win = lj_window_create(&lj_form_TGLOBALPATCHFORM, lj_gp_bindings);
    if (!gp->win) { free(gp); gp = NULL; lj_log("global patch: cannot create TGlobalPatchForm"); return NULL; }
    if ((clb = find("clbGroups")) != NULL) {
        for (k = 0; k < LJ_HTP_GROUPS; k++) items[k] = ctx->htp_names && ctx->htp_names->name[k][0] ? ctx->htp_names->name[k] : lj_gp_htp_items[k];
        lj_w_list_set_items(clb, items, LJ_HTP_GROUPS);
    }
    lj_menu_set_checked(lj_window_menus(gp->win), "M_HTPgroupsaffectedgyGM", lj_global_patch_htp_masters());
    fill_list();
    panel_fill();
    lj_win_show(gp->win, 0);
    return gp->win;
}

void lj_global_patch_close(void)
{
    if (!gp) return;
    lj_win_hide(gp->win);
    lj_window_destroy(gp->win);
    free(gp);
    gp = NULL;
}
