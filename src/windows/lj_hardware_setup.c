/* lj_hardware_setup.c - see lj_hardware_setup.h. The sACN controls are code-built
 * (design 7.7) inside the DMX Hardware page's Panel3 under pConfiguredHardware; the
 * geometry is ours (PROPOSAL rig M-hw: LJ2's page held Martin's card panels). */
#define _CRT_SECURE_NO_WARNINGS 1   /* sscanf/strcat under /W4 /WX on MSVC (as core/lj_ini.c) */
#include "windows/lj_hardware_setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "io/lj_net.h"
#include "ui/forms/lj_forms_gen.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lj.h"
#include "windows/lj_warnings.h"

#define LJ_HS_NICS 16
#define LJ_HS_X    150                    /* control column; labels at x 4 */
#define LJ_HS_ROW  28

typedef struct {
    lj_window *win;
    lj_sacn_cfg local;                    /* used when ctx->sacn is NULL (tests) */
    lj_nic nics[LJ_HS_NICS]; int nnics;
    char nic_text[LJ_HS_NICS][96]; const char *nic_items[LJ_HS_NICS];
    char status[160], addr_text[32];
    uint32_t pps[LJ_LINKS]; int last_err;
    lj_widget *nic, *uni[LJ_LINKS], *prio, *name, *preview, *unlock, *ucast[LJ_LINKS], *lstatus;
} lj_hs_state;
static lj_hs_state hs;

lj_window *lj_hardware_setup_window(void) { return hs.win; }
static lj_sacn_cfg *cfg(void) { lj_ctx *c = lj_ctx_get(); return c->sacn ? c->sacn : &hs.local; }
static lj_widget *find(const char *n) { return hs.win ? lj_window_find(hs.win, n) : NULL; }

static void ip_text(uint32_t ip, char *out, size_t cap) { snprintf(out, cap, "%u.%u.%u.%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); }
static int ip_parse(const char *s, uint32_t *out)
{
    unsigned a, b, c, d; char tail;
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return -1;
    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

/* ---- the code-built page (hand-filled descs, design 7.7) ---- */
static lj_widget *label(lj_widget *parent, lj_ctl_desc *d, const char *name, int x, int y, int w, const char *caption)
{
    memset(d, 0, sizeof *d);
    d->kind = LJ_K_LABEL; d->name = name; d->left = x; d->top = y; d->width = w; d->height = 13; d->caption = caption; d->font_inherited = 1; d->transparent = LJ_YES;
    return lj_w_label_create(parent, d);
}
static lj_widget *build_page(lj_widget *panel)
{
    static lj_ctl_desc d[32];
    static char lbl_u[LJ_LINKS][24], lbl_c[LJ_LINKS][24];
    static const char *const nic_label = "Network interface", *const prio_label = "Priority (0..200)", *const name_label = "Source name";
    int i, n = 0, y = 110;
    label(panel, &d[n++], "lNicLabel", 4, y + 4, 140, nic_label);
    memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_COMBOBOX; d[n].name = "cbNic"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 320; d[n].height = 21; d[n].style = "csDropDownList"; d[n].item_index = -1; d[n].drop_down_count = 8; d[n].item_height = 13; d[n].font_inherited = 1;
    hs.nic = lj_w_combobox_create(panel, &d[n++]);
    for (i = 0; i < LJ_LINKS; i++) {
        y += LJ_HS_ROW;
        snprintf(lbl_u[i], sizeof lbl_u[i], "Link %d -> Universe", i + 1);
        label(panel, &d[n++], "lUniverse", 4, y + 4, 140, lbl_u[i]);
        memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_SPINEDIT; d[n].name = i == 0 ? "seU1" : i == 1 ? "seU2" : i == 2 ? "seU3" : "seU4"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 80; d[n].height = 22; d[n].min_value = 0; d[n].max_value = LJ_SACN_UNIVERSE_MAX; d[n].max_length = 5; d[n].font_inherited = 1;
        hs.uni[i] = lj_w_spinedit_create(panel, &d[n++]);
    }
    y += LJ_HS_ROW;
    label(panel, &d[n++], "lPriority", 4, y + 4, 140, prio_label);
    memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_SPINEDIT; d[n].name = "sePriority"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 80; d[n].height = 22; d[n].min_value = 0; d[n].max_value = 255; d[n].max_length = 3; d[n].font_inherited = 1;   /* 255 so a bad value reaches the validation path, never the wire */
    hs.prio = lj_w_spinedit_create(panel, &d[n++]);
    y += LJ_HS_ROW;
    label(panel, &d[n++], "lName", 4, y + 4, 140, name_label);
    memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_EDIT; d[n].name = "eName"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 320; d[n].height = 21; d[n].max_length = LJ_SACN_NAME_CAP - 1; d[n].font_inherited = 1;
    hs.name = lj_w_edit_create(panel, &d[n++]);
    y += LJ_HS_ROW;
    memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_CHECKBOX; d[n].name = "cbPreview"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 300; d[n].height = 17; d[n].caption = "Preview data (not for live output)"; d[n].font_inherited = 1;
    hs.preview = lj_w_checkbox_create(panel, &d[n++]);
    y += 18;
    memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_CHECKBOX; d[n].name = "cbUnlockRate"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 300; d[n].height = 17; d[n].caption = "Unlock packet rate (above 44 Hz)"; d[n].font_inherited = 1;
    hs.unlock = lj_w_checkbox_create(panel, &d[n++]);
    for (i = 0; i < LJ_LINKS; i++) {
        y += LJ_HS_ROW;
        snprintf(lbl_c[i], sizeof lbl_c[i], "Unicast link %d", i + 1);
        label(panel, &d[n++], "lUnicast", 4, y + 4, 140, lbl_c[i]);
        memset(&d[n], 0, sizeof d[n]); d[n].kind = LJ_K_EDIT; d[n].name = i == 0 ? "eUnicast1" : i == 1 ? "eUnicast2" : i == 2 ? "eUnicast3" : "eUnicast4"; d[n].left = LJ_HS_X; d[n].top = y; d[n].width = 420; d[n].height = 21; d[n].max_length = 8 * 16; d[n].hint = "up to 8 addresses a.b.c.d, comma separated; empty = multicast"; d[n].font_inherited = 1;
        hs.ucast[i] = lj_w_edit_create(panel, &d[n++]);
    }
    y += LJ_HS_ROW + 8;
    hs.lstatus = label(panel, &d[n++], "lSacnStatus", 4, y, 900, "");
    return panel;
}

static void page_fill(void)
{
    const lj_sacn_cfg *c = cfg();
    char buf[8 * 20];
    int i, j, sel = -1;
    hs.nnics = lj_net_list_ipv4(hs.nics, LJ_HS_NICS);
    if (hs.nnics < 0) hs.nnics = 0;
    for (i = 0; i < hs.nnics; i++) {
        char ip[32]; ip_text(hs.nics[i].ipv4, ip, sizeof ip);
        snprintf(hs.nic_text[i], sizeof hs.nic_text[i], "%s (%s)%s", hs.nics[i].name, ip, hs.nics[i].up ? "" : " - down");
        hs.nic_items[i] = hs.nic_text[i];
        if (hs.nics[i].ipv4 == c->nic_ipv4) sel = i;
    }
    lj_w_combobox_set_items(hs.nic, hs.nic_items, hs.nnics);
    ((lj_w_combobox *)hs.nic)->index = sel;
    for (i = 0; i < LJ_LINKS; i++) {
        lj_w_spinedit_set_value(hs.uni[i], c->universe_of_link[i]);
        buf[0] = 0;
        for (j = 0; j < c->nunicast[i] && j < LJ_SACN_UNICAST_MAX; j++) { char ip[32]; ip_text(c->unicast[i][j], ip, sizeof ip); if (j) strcat(buf, ", "); strcat(buf, ip); }
        lj_w_edit_set_text(hs.ucast[i], buf);
    }
    lj_w_spinedit_set_value(hs.prio, c->priority);
    lj_w_edit_set_text(hs.name, c->name);
    lj_w_checkbox_set_checked(hs.preview, c->preview);
    lj_w_checkbox_set_checked(hs.unlock, c->unlock_rate);
    ip_text(c->nic_ipv4, hs.addr_text, sizeof hs.addr_text);
    lj_w_set_caption(find("lType"), "sACN (E1.31)");                 /* where Martin's cards were [08 s7] */
    lj_w_set_caption(find("lAddress"), hs.addr_text);
    lj_w_set_caption(find("lChannels"), "2048");
    lj_hardware_setup_refresh();
}

void lj_hardware_setup_status(const uint32_t pps[LJ_LINKS], int last_err) { if (pps) memcpy(hs.pps, pps, sizeof hs.pps); hs.last_err = last_err; }

void lj_hardware_setup_refresh(void)
{
    const lj_sacn_cfg *c = cfg();
    char ip[32], part[48];
    int i;
    if (!hs.win) return;
    ip_text(c->nic_ipv4, ip, sizeof ip);
    snprintf(hs.status, sizeof hs.status, "NIC %s", ip);
    for (i = 0; i < LJ_LINKS; i++) {
        if (!c->universe_of_link[i]) continue;
        snprintf(part, sizeof part, " | U%u %u pkt/s", (unsigned)c->universe_of_link[i], (unsigned)hs.pps[i]);
        strncat(hs.status, part, sizeof hs.status - strlen(hs.status) - 1);
    }
    snprintf(part, sizeof part, " | last send error %d", hs.last_err);
    strncat(hs.status, part, sizeof hs.status - strlen(hs.status) - 1);
    lj_w_set_caption(hs.lstatus, hs.status);
    lj_w_set_caption(find("lStatus"), hs.last_err ? "send error" : "ok");
}

int lj_hardware_setup_set_priority(int priority) { return hs.win ? lj_w_spinedit_set_value(hs.prio, priority) : -1; }

const char *lj_hardware_setup_apply(void)
{
    static char err[96];
    lj_ctx *ctx = lj_ctx_get();
    lj_sacn_cfg n = *cfg();
    const char *msg = NULL;
    int i, idx, prio;
    char list[8 * 20], *tok, *end;
    if (!hs.win) return "Hardware Setup is not open";
    idx = ((lj_w_combobox *)hs.nic)->index;
    if (idx >= 0 && idx < hs.nnics) n.nic_ipv4 = hs.nics[idx].ipv4;
    for (i = 0; i < LJ_LINKS; i++) {
        n.universe_of_link[i] = (uint16_t)lj_w_spinedit_value(hs.uni[i]);
        n.nunicast[i] = 0;
        lj_str_copy(list, sizeof list, lj_w_edit_text(hs.ucast[i]) ? lj_w_edit_text(hs.ucast[i]) : "");
        for (tok = list; *tok && !msg; tok = end) {                      /* comma/space separated, no strtok (C11 Annex K is optional) */
            while (*tok == ',' || *tok == ' ') tok++;
            if (!*tok) break;
            for (end = tok; *end && *end != ',' && *end != ' '; end++) { }
            if (*end) *end++ = 0;
            if (n.nunicast[i] >= LJ_SACN_UNICAST_MAX) { snprintf(err, sizeof err, "Link %d: more than %d unicast addresses", i + 1, LJ_SACN_UNICAST_MAX); msg = err; }
            else if (ip_parse(tok, &n.unicast[i][n.nunicast[i]]) != 0) { snprintf(err, sizeof err, "Link %d: unicast address %.20s is not a.b.c.d", i + 1, tok); msg = err; }
            else n.nunicast[i]++;
        }
        if (msg) break;
    }
    prio = lj_w_spinedit_value(hs.prio);
    if (!msg && (prio < 0 || prio > LJ_SACN_PRIORITY_MAX)) { snprintf(err, sizeof err, "sACN priority must be 0..%d (ME-24)", LJ_SACN_PRIORITY_MAX); msg = err; }   /* lj_sacn_cfg_validate clamps; the page rejects so the user sees it */
    n.priority = (uint8_t)prio;
    lj_str_copy(n.name, sizeof n.name, lj_w_edit_text(hs.name) ? lj_w_edit_text(hs.name) : "");
    n.preview = (uint8_t)lj_w_checkbox_checked(hs.preview);
    n.unlock_rate = (uint8_t)lj_w_checkbox_checked(hs.unlock);
    if (!msg) msg = lj_sacn_cfg_validate(&n);
    if (msg) { lj_win_message("Hardware Setup", msg); return msg; }
    lj_lock(); *cfg() = n; lj_unlock();                               /* the engine thread takes this pointer with lj_engine_take_sacn_cfg (ME-23) */
    if (ctx->sacn) { lj_cmd c; memset(&c, 0, sizeof c); c.kind = LJ_CMD_SACN_CFG; c.sacn = ctx->sacn; lj_ctx_post(&c); }
    if (ctx->hardware_ini) {
        char path[LJ_PATH_CAP];
        lj_sacn_cfg_save(ctx->hardware_ini, cfg());
        if (snprintf(path, sizeof path, "%sHardware.Ini", ctx->userdir) < (int)sizeof path && lj_ini_save(ctx->hardware_ini, path) != 0) lj_log("hardware setup: cannot write %s", path);
    }
    page_fill();
    return NULL;
}

/* ---- handlers ---- */
static void noop(lj_widget *w, void *ud) { (void)w; (void)ud; }
static void na(lj_widget *w, void *ud) { (void)ud; lj_log("hardware setup: %s: Not available in LJ4EVR", w && w->name ? w->name : "?"); }
static void on_ok(lj_widget *w, void *ud) { (void)w; (void)ud; if (lj_hardware_setup_apply() == NULL) lj_hardware_setup_close(); }

static const lj_binding lj_hs_bindings[] = {
    { "FormActivate", noop, NULL }, { "FormCreate", noop, NULL }, { "FormDestroy", noop, NULL }, { "FormDeactivate", noop, NULL }, { "PC1Change", noop, NULL },
    { "bOkClick", on_ok, NULL }, { "bdetectHardwareClick", na, NULL }, { "bClearhardwareClick", na, NULL }, { "cbUseDMXinClick", na, NULL },
    { "RGrefreshClick", noop, NULL }, { "lvUSBDblClick", noop, NULL }, { "lvUSBSelectItem", noop, NULL }, { "sbID1MouseDown", noop, NULL }, { "sbID1MouseUp", noop, NULL },
    { "sbWinampClick", na, NULL }, { "bMTCSeelctClick", na, NULL }, { "RG2532EnableClick", noop, NULL }, { "rg2518EnableClick", noop, NULL }, { "Help1Click", noop, NULL },
    { NULL, NULL, NULL }
};

lj_window *lj_hardware_setup_open(void)
{
    static const char *const hidden[] = { "pUSB", "p4064Club", "p4064DJ", "pPCMCIA1", "pPCI", "pPCMCIA2" };
    static const char *const greyed[] = { "bdetectHardware", "bClearhardware", "cbUseDMXin" };
    static const char *const blank[] = { "lProtectionDeviceStatus", "lLicenseStatus", "Label10" };   /* D3: the licence lines are gone */
    lj_widget *panel, *w;
    size_t i;
    if (hs.win) return hs.win;
    hs.win = lj_window_create(&lj_form_THARDWARESETUPFORM2, lj_hs_bindings);
    if (!hs.win) { lj_log("hardware setup: cannot create THardwareSetupForm2"); return NULL; }
    for (i = 0; i < LJ_COUNT_OF(hidden); i++) if ((w = find(hidden[i])) != NULL) w->visible = 0;
    for (i = 0; i < LJ_COUNT_OF(greyed); i++) if ((w = find(greyed[i])) != NULL) { w->enabled = 0; w->hint = "Not available in LJ4EVR"; }
    for (i = 0; i < LJ_COUNT_OF(blank); i++) lj_w_set_caption(find(blank[i]), "");
    panel = find("Panel3");
    if (!panel) { lj_hardware_setup_close(); return NULL; }
    build_page(panel);
    page_fill();
    lj_win_show(hs.win, 0);
    return hs.win;
}

void lj_hardware_setup_close(void)
{
    if (!hs.win) return;
    lj_win_hide(hs.win);
    lj_window_destroy(hs.win);
    memset(&hs, 0, sizeof hs);
}
