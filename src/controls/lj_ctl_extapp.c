/* lj_ctl_extapp.c - TSpecialSettings_ExternalApp (controls/lj_ctl_extapp.h). Geometry: no capture of this window exists
 * (no GIF, no DFM: the class is code-built in LJ2 too), so every number is PROPOSAL rig W5-EXT-1, sized on the
 * MS Sans Serif 8 metrics of the other control windows (TEdit 21 high, TButton 25 high as the DFMs use). */
#include "controls/lj_ctl_extapp.h"

#include <string.h>
#include "core/lj_lock.h"
#include "core/lj_log.h"
#include "core/lj_str.h"
#include "io/lj_extapp.h"
#include "ui/widgets/lj_widgets_basic.h"

enum { LJ_EXT_CW = 340, LJ_EXT_CH = 92, LJ_EXT_BOX_W = 332, LJ_EXT_BOX_H = 84, LJ_EXT_EDIT_W = 316, LJ_EXT_BTN_W = 81 };   /* PROPOSAL rig W5-EXT-1 */

static const char *link_text(int fixture1)
{
    const lj_extapp_cfg *cfg = lj_ctx_get()->extapp;
    lj_extapp_stats st;
    int i = lj_extapp_slot_of(cfg, fixture1);
    if (i < 0 || cfg->link[i].transport == LJ_EXTAPP_OFF) return "Application Link Undefined";   /* exe 0x3328BC */
    lj_extapp_stats_get(&st);
    return st.connected[i] ? "Link: Active" : "Link:-";             /* exe 0x24F6A4, 0x24F158 */
}

const char *lj_ctl_extapp_get(int fixture)
{
    static char out[LJ_EXT_CMD_CAP];
    lj_scene *s;
    int j;
    out[0] = 0;
    lj_lock();
    s = lj_ctx_scene();
    for (j = 0; s && j < LJ_SCENE_EXT; j++) if (s->ext[j].fixture == fixture + 1) lj_str_copy(out, sizeof out, s->ext[j].cmd);
    lj_unlock();
    return out;
}

int lj_ctl_extapp_set(int fixture, const char *cmd)
{
    const char *bad = cmd && cmd[0] ? lj_extapp_validate(cmd) : NULL;
    char err[64];
    uint8_t bytes[LJ_EXTAPP_MAX_BYTES];
    size_t n;
    lj_scene *s;
    int j, at = -1, rc = -1;
    if (fixture < 0 || fixture >= LJ_FIXTURES || !cmd) return -1;
    if (!bad && cmd[0] && lj_extapp_encode(cmd, bytes, &n, err, sizeof err) != 0) bad = err;   /* #256 and friends: refused at the edit, not only at send */
    if (bad) { lj_log("extapp: \"%s\" refused: %s", cmd, bad); return -1; }
    lj_lock();
    if ((s = lj_ctx_scene()) != NULL) {
        for (j = 0; j < LJ_SCENE_EXT && at < 0; j++) if (s->ext[j].fixture == fixture + 1) at = j;
        if (at < 0 && cmd[0]) for (j = 0; j < LJ_SCENE_EXT && at < 0; j++) if (!s->ext[j].fixture) at = j;
        if (at >= 0) {
            if (cmd[0]) { s->ext[at].fixture = (uint8_t)(fixture + 1); lj_str_copy(s->ext[at].cmd, sizeof s->ext[at].cmd, cmd); }
            else memset(&s->ext[at], 0, sizeof s->ext[at]);
            lj_ctx_get()->show->current_seq->modified = 1;
        }
        rc = at >= 0 || !cmd[0] ? 0 : -1;
    }
    lj_unlock();
    if (rc == 0 && at >= 0) lj_ctx_set_modified(LJ_MOD_SEQ, 1);
    if (rc != 0) lj_log("extapp: no current scene, or %d External Application strings in it already", (int)LJ_SCENE_EXT);
    return rc;
}

static lj_widget *find(lj_ctl_win *c, const char *name) { return lj_ctl_elem_find(c->win, name); }

static void on_btn(lj_widget *w, int ev, void *ud)
{
    lj_ctl_win *c = lj_ctl_of(lj_window_of(w));
    lj_widget *ed;
    int m = lj_sel_master();
    (void)ud;
    if (ev != LJ_WEV_CLICK || !c || m < 0 || (ed = find(c, "ext_cmd")) == NULL) return;
    if (!strcmp(w->name, "ext_set")) lj_ctl_extapp_set(m, lj_w_edit_text(ed));
    else lj_w_edit_set_text(ed, lj_ctl_extapp_get(m));
}

static lj_widget *add(lj_ctl_win *c, lj_widget *parent, const char *name, enum lj_ctl_row_kind kind, const char *caption, int x, int y, int w, int h)
{
    lj_ctl_elem *e = lj_ctl_elem_add(c, parent, NULL, NULL, LJ_CE_NONE);
    lj_ctl_desc *d;
    lj_widget *wd;
    if (!e) return NULL;
    d = lj_ctl_desc_new(e, kind, caption, x, y, w, h);
    d->name = name;
    if (kind == LJ_K_EDIT) d->max_length = LJ_EXTAPP_MAX_BYTES;
    wd = kind == LJ_K_GROUPBOX ? lj_w_groupbox_create(parent, d) : kind == LJ_K_LABEL ? lj_w_label_create(parent, d)
       : kind == LJ_K_EDIT ? lj_w_edit_create(parent, d) : lj_w_button_create(parent, d);
    e->w = wd;
    lj_str_copy(e->name, sizeof e->name, name);                    /* the --script name (lj_ctl_elem_find) */
    if (kind == LJ_K_BUTTON) { wd->on_event = on_btn; wd->ud = e; }
    return wd;
}

void lj_ctl_extapp_build(lj_ctl_win *c)
{
    lj_widget *root = lj_window_root(c->win), *box;
    int m = lj_sel_master();
    box = add(c, root, "ext_box", LJ_K_GROUPBOX, "ZString Interface", 4, 4, LJ_EXT_BOX_W, LJ_EXT_BOX_H);   /* exe 0x24EB58 */
    if (!box) return;
    add(c, box, "ext_label", LJ_K_LABEL, "External Application Command", 8, 16, LJ_EXT_EDIT_W, 13);       /* exe 0x24F168 */
    add(c, box, "ext_cmd", LJ_K_EDIT, "", 8, 31, LJ_EXT_EDIT_W, 21);
    add(c, box, "ext_set", LJ_K_BUTTON, "Set Command", 8, 55, LJ_EXT_BTN_W, 25);                            /* exe 0x24F194 */
    add(c, box, "ext_get", LJ_K_BUTTON, "Get Command", 8 + LJ_EXT_BTN_W + 4, 55, LJ_EXT_BTN_W, 25);         /* exe 0x24F1A8 */
    add(c, box, "ext_link", LJ_K_LABEL, "Link:-", 8 + 2 * (LJ_EXT_BTN_W + 4) + 4, 61, LJ_EXT_EDIT_W - 2 * (LJ_EXT_BTN_W + 4) - 4, 13);
    if (m >= 0) lj_w_edit_set_text(find(c, "ext_cmd"), lj_ctl_extapp_get(m));
    lj_window_resize(c->win, LJ_EXT_CW, LJ_EXT_CH);
    lj_ctl_extapp_repaint(c);
}

void lj_ctl_extapp_repaint(lj_ctl_win *c)
{
    lj_widget *l = find(c, "ext_link");
    const char *t = link_text(lj_sel_master() + 1);
    const char *now = l ? lj_w_caption(l) : NULL;
    if (l && (!now || strcmp(now, t))) { lj_w_set_caption(l, t); lj_widget_invalidate(l); }
}
