/* lj_remote_form.c - the REMOTE family (windows/lj_remote_form.h): TRS232SettingsForm "RS232 Settings" from LJ_RS232.exe.
 * The exe's TPF0 stream is harvested by hand here (the side exe is not in the dfm2c corpus, W5-X-HARVEST is the
 * integrator's): rgPort, rgBaudRate, Panel1 with bOk / bCancel and the Help menu are the DFM's numbers and captions
 * verbatim (Width 309 Height 203, client 301 x 157). LJ2 had no TCP and no enable switch (the side exe listened while it
 * ran): gbRemote below the two radio groups is ours, every number tagged PROPOSAL rig W5-REM-4. OK stores ctx->remote and
 * runs LJ_ACT_IO_APPLY (LJ_IO_REMOTE: Hardware.Ini [REMOTE] saved, the transports reopened).
 * Also installs the io/lj_remote.h state query (the view, the Statics 2 page, the cue-list status).
 * Handlers: FormActivate RGClick bOkClick bCancelClick Help1Click (ME-16). */
#include "windows/lj_remote_form.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "desktop/lj_desktop.h"
#include "io/lj_net.h"
#include "io/lj_remote.h"
#include "io/lj_serial.h"
#include "io/lj_socket.h"
#include "ui/widgets/lj_widgets_basic.h"
#include "ui/widgets/lj_widgets_lists.h"
#include "windows/lj_help_viewer.h"
#include "windows/lj_statics2_form.h"

/* ---- TRS232SettingsForm (LJ_RS232.exe TPF0 at 0x5D480; 0x5C8E0 carries the same form after TLJRS232Main) ---- */
static const char *const rf_port_items[] = { "COM 1", "COM 2", "COM 3", "COM 4" };
static const char *const rf_baud_items[] = { "2400", "4800", "9600", "14400", "19200", "38400", "57600" };
static const lj_ctl_desc rf_panel1[] = {
    { LJ_K_BITBTN, "bOk", 72, 3, 75, 25, .caption = "&Ok", .enabled = LJ_NO, .tab_order = 0, .kind_str = "bkOK", .font = LJ_FONT_SANS13, .font_inherited = 1, .num_glyphs = 1, .spacing = 4, .events = { { "OnClick", "bOkClick" } } },
    { LJ_K_BITBTN, "bCancel", 152, 3, 75, 25, .caption = "&Cancel", .tab_order = 1, .kind_str = "bkCancel", .font = LJ_FONT_SANS13, .font_inherited = 1, .num_glyphs = 1, .spacing = 4, .events = { { "OnClick", "bCancelClick" } } },
};
/* gbRemote: PROPOSAL rig W5-REM-4 (ours; LJ2's side exe has no such group) */
static const lj_ctl_desc rf_remote[] = {
    { LJ_K_CHECKBOX, "cbSerial", 8, 16, 185, 17, .caption = "RS-232 remote enabled", .tab_order = 0, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnClick", "RGClick" } } },
    { LJ_K_CHECKBOX, "cbTcp", 8, 36, 185, 17, .caption = "TCP remote enabled", .tab_order = 1, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnClick", "RGClick" } } },
    { LJ_K_LABEL, "lInterface", 8, 62, 45, 13, .caption = "Interface", .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_COMBOBOX, "cbInterface", 60, 58, 137, 21, .style = "csDropDownList", .item_index = -1, .item_height = 13, .drop_down_count = 8, .tab_order = 2, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnChange", "RGClick" } } },
    { LJ_K_LABEL, "lTcpPort", 206, 62, 19, 13, .caption = "Port", .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_EDIT, "eTcpPort", 232, 58, 57, 21, .max_length = 5, .tab_order = 3, .font = LJ_FONT_SANS13, .font_inherited = 1, .events = { { "OnChange", "RGClick" } } },
    { LJ_K_LABEL, "lNoAuth1", 8, 86, 281, 13, .caption = "No authentication: on a network interface,", .font = LJ_FONT_SANS13, .font_inherited = 1 },
    { LJ_K_LABEL, "lNoAuth2", 8, 100, 281, 13, .caption = "anyone on that network can drive the show.", .font = LJ_FONT_SANS13, .font_inherited = 1 },
};
static const lj_ctl_desc rf_ctls[] = {
    { LJ_K_RADIOGROUP, "rgPort", 0, 0, 113, 105, .caption = "COM Port", .item_index = -1, .tab_order = 0, .font = LJ_FONT_SANS13, .font_inherited = 1, .items = rf_port_items, .nitems = LJ_COUNT_OF(rf_port_items), .events = { { "OnClick", "RGClick" } } },
    { LJ_K_RADIOGROUP, "rgBaudRate", 112, 0, 185, 105, .caption = "Baud Rate", .columns_count = 2, .item_index = -1, .tab_order = 1, .font = LJ_FONT_SANS13, .font_inherited = 1, .items = rf_baud_items, .nitems = LJ_COUNT_OF(rf_baud_items), .events = { { "OnClick", "RGClick" } } },
    { LJ_K_GROUPBOX, "gbRemote", 0, 105, 297, 114, .caption = "Remote", .tab_order = 3, .font = LJ_FONT_SANS13, .font_inherited = 1, .children = rf_remote, .nchildren = LJ_COUNT_OF(rf_remote) },   /* PROPOSAL rig W5-REM-4 */
    { LJ_K_PANEL, "Panel1", 0, 240, 301, 31, .align = LJ_AL_BOTTOM, .bevel_inner = LJ_BV_RAISED, .bevel_outer = LJ_BV_LOWERED, .tab_order = 2, .font = LJ_FONT_SANS13, .font_inherited = 1, .bevel_width = 1, .children = rf_panel1, .nchildren = LJ_COUNT_OF(rf_panel1) },
    { LJ_K_MAINMENU, "MainMenu1", 24, 8, 0, 0, .font = LJ_FONT_SANS13, .font_inherited = 1 },
};
static const lj_menu_item_desc rf_menu1[] = { { .name = "Help1", .caption = "&Help", .on_click = "Help1Click", .image_index = -1 } };
static const lj_menu_desc rf_menus[] = { { "MainMenu1", 1, rf_menu1, 1 } };
static const lj_form_desc lj_form_TRS232SETTINGSFORM = {
    .name = "RS232SettingsForm", .class_name = "TRS232SettingsForm", .caption = "RS232 Settings",
    .left = 407, .top = 335, .width = 309, .height = 203 + 114,           /* the DFM's 203 plus gbRemote (PROPOSAL rig W5-REM-4) */
    .color = LJ_CLR(0xC0C0C0), .font_color = LJ_CLR(0x000000), .menu = "MainMenu1", .position = LJ_PO_SCREENCENTER,
    .text_height = 13, .scaled = LJ_NO, .autosize = LJ_YES, .font = LJ_FONT_SANS13, .font_name = "MS Sans Serif", .font_height = -11, .font_style = 0,
    .client_width = 301, .client_height = 157 + 114, .client_derived = 1,
    .ctls = rf_ctls, .n = LJ_COUNT_OF(rf_ctls), .menus = rf_menus, .nmenus = LJ_COUNT_OF(rf_menus),
    .events = { { "OnActivate", "FormActivate" } }
};

enum { LJ_RF_NICS = 16 };
static struct {
    lj_window *win;
    lj_serial_port ports[4]; int nports;                             /* POSIX: the enumerated devices shown in rgPort */
    uint32_t ips[LJ_RF_NICS + 1]; char iptxt[LJ_RF_NICS + 1][40]; const char *ipitems[LJ_RF_NICS + 1]; int nips;
    char porttxt[8];
} rf;

static lj_widget *find(const char *n) { return rf.win ? lj_window_find(rf.win, n) : NULL; }
static void enable_ok(void) { lj_widget *w = find("bOk"); if (w && !w->enabled) { w->enabled = 1; lj_widget_invalidate(w); } }

static void fill(void)
{
    const lj_remote_cfg *c = lj_ctx_get()->remote;
    lj_widget *rg = find("rgPort"), *w;
    lj_nic nic[LJ_RF_NICS];
    int i, n, sel = 0;
    if (!c) return;
    rf.nports = lj_serial_list(rf.ports, 4);
    if (rf.nports > 0 && !strncmp(rf.ports[0].name, "/dev/", 5) && rg) {   /* POSIX: the devices present stand in for COM 1-4 (PROPOSAL rig W5-REM-3) */
        lj_widget *r = rg->first;
        for (i = 0; r && i < 4; i++, r = r->next) {
            if (i < rf.nports) lj_w_set_caption(r, rf.ports[i].name); else r->visible = 0;
        }
    } else rf.nports = 0;
    for (i = 0; i < 4; i++) {
        char com[8];
        snprintf(com, sizeof com, "COM%d", i + 1);
        if (rf.nports ? (i < rf.nports && !strcmp(c->serial_port, rf.ports[i].name)) : !strcmp(c->serial_port, com)) lj_w_radiogroup_set_index(rg, i);
    }
    for (i = 0; i < LJ_REMOTE_BAUDS; i++) if (lj_remote_bauds[i] == c->baud) lj_w_radiogroup_set_index(find("rgBaudRate"), i);
    rf.nips = 0;
    rf.ips[rf.nips] = LJ_IP_LOOPBACK; lj_str_copy(rf.iptxt[rf.nips], sizeof rf.iptxt[0], "127.0.0.1 (local)"); rf.nips++;
    n = lj_net_list_ipv4(nic, LJ_RF_NICS);
    for (i = 0; i < n && rf.nips <= LJ_RF_NICS; i++) {
        uint32_t ip = nic[i].ipv4;
        if (nic[i].loopback || !nic[i].up || !ip) continue;
        rf.ips[rf.nips] = ip;
        snprintf(rf.iptxt[rf.nips], sizeof rf.iptxt[0], "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255u, (ip >> 8) & 255u, ip & 255u);
        if (ip == c->tcp_ip) sel = rf.nips;
        rf.nips++;
    }
    for (i = 0; i < rf.nips; i++) rf.ipitems[i] = rf.iptxt[i];
    if ((w = find("cbInterface")) != NULL) { lj_w_combobox_set_items(w, rf.ipitems, rf.nips); ((lj_w_combobox *)w)->index = sel; lj_widget_invalidate(w); }
    snprintf(rf.porttxt, sizeof rf.porttxt, "%u", (unsigned)c->tcp_port);
    lj_w_edit_set_text(find("eTcpPort"), rf.porttxt);
    lj_w_checkbox_set_checked(find("cbSerial"), c->serial_on);
    lj_w_checkbox_set_checked(find("cbTcp"), c->tcp_on);
    if ((w = find("bOk")) != NULL) { w->enabled = 0; lj_widget_invalidate(w); }   /* the DFM's Enabled = False until a change (RGClick) */
}

static void close_self(void) { lj_remote_form_close(LJ_WIN_REMOTE_SETUP); }
static void on_change(lj_widget *w, void *ud) { (void)w; (void)ud; enable_ok(); }
static void on_cancel(lj_widget *w, void *ud) { (void)w; (void)ud; close_self(); }

static void on_ok(lj_widget *w, void *ud)
{
    lj_remote_cfg *c = lj_ctx_get()->remote;
    const char *pt = lj_w_edit_text(find("eTcpPort"));
    lj_widget *cb = find("cbInterface");
    int p = lj_w_radiogroup_index(find("rgPort")), b = lj_w_radiogroup_index(find("rgBaudRate")), ix = cb ? ((lj_w_combobox *)cb)->index : 0;
    long port = 0;
    char *end = NULL;
    (void)w; (void)ud;
    if (!c) { close_self(); return; }
    if (pt && *pt) port = strtol(pt, &end, 10);
    if (!pt || !*pt || !end || *end || port < 1 || port > 65535) { lj_win_message("RS232 Settings", "The TCP port must be a number from 1 to 65535."); return; }
    if (p >= 0 && p < 4) {
        if (rf.nports) { if (p < rf.nports) lj_str_copy(c->serial_port, sizeof c->serial_port, rf.ports[p].name); }
        else snprintf(c->serial_port, sizeof c->serial_port, "COM%d", p + 1);
    }
    if (b >= 0 && b < LJ_REMOTE_BAUDS) c->baud = lj_remote_bauds[b];
    c->tcp_ip = ix >= 0 && ix < rf.nips ? rf.ips[ix] : LJ_IP_LOOPBACK;
    c->tcp_port = (uint16_t)port;
    c->serial_on = (uint8_t)lj_w_checkbox_checked(find("cbSerial"));
    c->tcp_on = (uint8_t)lj_w_checkbox_checked(find("cbTcp"));
    lj_desktop_run_action(LJ_ACT_IO_APPLY, LJ_IO_REMOTE);
    close_self();
}

static const lj_binding rf_b[] = {
    { "FormActivate", lj_handler_noop, NULL }, { "RGClick", on_change, NULL }, { "bOkClick", on_ok, NULL }, { "bCancelClick", on_cancel, NULL },
    { "Help1Click", lj_help_viewer_click, "hotkeys-midi" },
    { NULL, NULL, NULL } };

lj_window *lj_remote_form_open(enum lj_win id)
{
    if (id != LJ_WIN_REMOTE_SETUP) return NULL;
    if (rf.win) { lj_win_show(rf.win, 0); return rf.win; }
    rf.win = lj_window_create(&lj_form_TRS232SETTINGSFORM, rf_b);
    if (!rf.win) { lj_log("remote: cannot create TRS232SettingsForm"); return NULL; }
    fill();
    lj_win_show(rf.win, 0);
    return rf.win;
}

void lj_remote_form_close(enum lj_win id)
{
    if (id != LJ_WIN_REMOTE_SETUP || !rf.win) return;
    lj_win_hide(rf.win);
    lj_window_destroy(rf.win);
    memset(&rf, 0, sizeof rf);
}

lj_window *lj_remote_form_get(enum lj_win id) { return id == LJ_WIN_REMOTE_SETUP ? rf.win : NULL; }
void       lj_remote_form_refresh(enum lj_win id) { (void)id; }

/* ---- the state query behind the set-style commands (io/lj_remote.h) ---- */
static int rf_query(enum lj_remote_q q, int n)
{
    lj_ctx *c = lj_ctx_get();
    const lj_engine_status *v = &c->view;
    int page;
    switch (q) {
    case LJ_REMQ_SMOKE_TIMER: return v->tick_no ? v->smoke_timer != 0 : -1;
    case LJ_REMQ_STATIC:      return n >= 1 && n <= 20 ? lj_statics2_slot_active(n) : -1;
    case LJ_REMQ_STATIC_FLASH:
        page = lj_statics2_page();
        if (!c->show || !c->show->pages || page < 1 || page > c->show->npages || n < 1 || n > LJ_STATICS2_SLOTS_MAX) return -1;
        return c->show->pages[page - 1].slot[n - 1].activation == LJ_ACT_FLASH;
    case LJ_REMQ_SLOT:        return v->tick_no && n >= 1 && n <= LJ_CUE_SLOTS ? v->slot_status[n - 1] != LJ_SLOT_DISABLED : -1;
    case LJ_REMQ_BGSLOT:      return v->tick_no && n >= 1 && n <= LJ_BG_SLOTS ? v->bg_enabled[n - 1] != 0 : -1;
    case LJ_REMQ_CL_MANUAL:   return c->cl && c->cl->state == LJ_CL_WAIT_GO;
    default:                  return -1;
    }
}

void lj_remote_form_bind_fns(void) { lj_remote_set_query(rf_query); }
